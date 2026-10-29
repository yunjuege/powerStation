#include "pfr/compute/RealtimeAggregator.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace {

int failedTests = 0;

void expect(bool condition, const std::string& name)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << '\n';
    }
    else
    {
        std::cout << "[FAIL] " << name << '\n';
        ++failedTests;
    }
}

bool approximatelyEqual(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

pfr::domain::Unit makeUnit(
    const std::string& plantId,
    const std::string& unitId,
    double capacityMw,
    bool enabled = true)
{
    return {
        unitId,
        "grid-east",
        plantId,
        "Unit " + unitId,
        capacityMw,
        1.0,
        enabled
    };
}

pfr::domain::Sample makeSample(
    const std::string& plantId,
    const std::string& unitId,
    pfr::domain::TimePoint receivedAt,
    double activePowerMw,
    double frequencyHz,
    double agcCommandMw,
    pfr::domain::DataQuality quality)
{
    pfr::domain::Sample sample;

    sample.gridId = "grid-east";
    sample.plantId = plantId;
    sample.unitId = unitId;

    sample.receivedAt = receivedAt;
    sample.collectedAt =
        receivedAt - std::chrono::milliseconds(10);

    sample.activePowerMw = activePowerMw;
    sample.frequencyHz = frequencyHz;
    sample.agcCommandMw = agcCommandMw;
    sample.quality = quality;

    sample.sequence = 1;
    sample.source = "test-source";

    return sample;
}

pfr::compute::UnitRealtimeSnapshot makeSnapshot(
    pfr::domain::Unit unit,
    std::optional<pfr::domain::Sample> latest)
{
    pfr::compute::UnitRealtimeSnapshot result;
    result.unit = std::move(unit);
    result.latest = std::move(latest);
    return result;
}

const pfr::compute::GridRealtimeSnapshot* findGrid(
    const pfr::compute::RealtimeAggregationSnapshot& snapshot,
    const std::string& gridId)
{
    for (const auto& grid : snapshot.grids)
    {
        if (grid.gridId == gridId)
        {
            return &grid;
        }
    }

    return nullptr;
}

const pfr::compute::PlantRealtimeSnapshot* findPlant(
    const pfr::compute::RealtimeAggregationSnapshot& snapshot,
    const std::string& gridId,
    const std::string& plantId)
{
    for (const auto& plant : snapshot.plants)
    {
        if (plant.gridId == gridId
            && plant.plantId == plantId)
        {
            return &plant;
        }
    }

    return nullptr;
}

void testClassification()
{
    using namespace std::chrono;
    using pfr::compute::UnitRealtimeState;

    const auto now =
        pfr::domain::TimePoint{seconds(100)};

    pfr::compute::RealtimeAggregationOptions options;
    options.staleAfter = seconds(5);

    pfr::compute::RealtimeAggregator aggregator(options);

    const auto disabled = makeSnapshot(
        makeUnit("plant-a", "disabled", 100.0, false),
        std::nullopt);

    const auto noData = makeSnapshot(
        makeUnit("plant-a", "no-data", 100.0),
        std::nullopt);

    const auto fresh = makeSnapshot(
        makeUnit("plant-a", "fresh", 100.0),
        makeSample(
            "plant-a",
            "fresh",
            now - seconds(1),
            60.0,
            50.0,
            65.0,
            pfr::domain::DataQuality::Good));

    const auto stale = makeSnapshot(
        makeUnit("plant-a", "stale", 100.0),
        makeSample(
            "plant-a",
            "stale",
            now - seconds(10),
            60.0,
            50.0,
            65.0,
            pfr::domain::DataQuality::Good));

    const auto bad = makeSnapshot(
        makeUnit("plant-a", "bad", 100.0),
        makeSample(
            "plant-a",
            "bad",
            now - seconds(1),
            60.0,
            50.0,
            65.0,
            pfr::domain::DataQuality::Bad));

    expect(
        aggregator.classify(disabled, now)
            == UnitRealtimeState::Disabled,
        "disabled unit is classified");

    expect(
        aggregator.classify(noData, now)
            == UnitRealtimeState::NoData,
        "unit without sample is classified");

    expect(
        aggregator.classify(fresh, now)
            == UnitRealtimeState::Fresh,
        "recent good sample is fresh");

    expect(
        aggregator.classify(stale, now)
            == UnitRealtimeState::Stale,
        "old sample is stale");

    expect(
        aggregator.classify(bad, now)
            == UnitRealtimeState::BadQuality,
        "bad sample quality is classified");
}

void testPlantAndGridAggregation()
{
    using namespace std::chrono;

    const auto now =
        pfr::domain::TimePoint{seconds(100)};

    pfr::compute::RealtimeAggregationOptions options;
    options.staleAfter = seconds(5);

    pfr::compute::RealtimeAggregator aggregator(options);

    std::vector<pfr::compute::UnitRealtimeSnapshot> units;

    units.push_back(makeSnapshot(
        makeUnit("plant-a", "unit-1", 100.0),
        makeSample(
            "plant-a",
            "unit-1",
            now - seconds(1),
            60.0,
            49.9,
            65.0,
            pfr::domain::DataQuality::Good)));

    units.push_back(makeSnapshot(
        makeUnit("plant-a", "unit-2", 200.0),
        makeSample(
            "plant-a",
            "unit-2",
            now - seconds(1),
            100.0,
            50.1,
            110.0,
            pfr::domain::DataQuality::Suspect)));

    units.push_back(makeSnapshot(
        makeUnit("plant-a", "unit-3", 150.0),
        makeSample(
            "plant-a",
            "unit-3",
            now - seconds(10),
            80.0,
            50.0,
            90.0,
            pfr::domain::DataQuality::Good)));

    units.push_back(makeSnapshot(
        makeUnit("plant-a", "unit-4", 50.0, false),
        std::nullopt));

    units.push_back(makeSnapshot(
        makeUnit("plant-b", "unit-1", 300.0),
        makeSample(
            "plant-b",
            "unit-1",
            now - seconds(1),
            200.0,
            50.0,
            210.0,
            pfr::domain::DataQuality::Bad)));

    units.push_back(makeSnapshot(
        makeUnit("plant-b", "unit-2", 400.0),
        std::nullopt));

    const auto result =
        aggregator.aggregate(units, now);

    const auto* plantA =
        findPlant(result, "grid-east", "plant-a");

    const auto* plantB =
        findPlant(result, "grid-east", "plant-b");

    expect(
        result.plants.size() == 2,
        "two plants are aggregated");

    expect(
        result.grids.size() == 1,
        "one grid is aggregated");

    expect(
        plantA != nullptr
            && plantA->metrics.totalUnits == 4
            && plantA->metrics.enabledUnits == 3
            && plantA->metrics.disabledUnits == 1
            && plantA->metrics.freshUnits == 2
            && plantA->metrics.staleUnits == 1
            && plantA->metrics.suspectUnits == 1,
        "plant A state counters are correct");

    expect(
        plantA != nullptr
            && approximatelyEqual(
                plantA->metrics.enabledRatedCapacityMw,
                450.0)
            && approximatelyEqual(
                plantA->metrics.onlineRatedCapacityMw,
                300.0)
            && approximatelyEqual(
                plantA->metrics.activePowerMw,
                160.0)
            && approximatelyEqual(
                plantA->metrics.agcCommandMw,
                175.0),
        "plant A power metrics are correct");

    expect(
        plantA != nullptr
            && plantA->metrics.averageFrequencyHz.has_value()
            && approximatelyEqual(
                plantA->metrics.averageFrequencyHz.value(),
                50.0)
            && plantA->metrics.loadRatio.has_value()
            && approximatelyEqual(
                plantA->metrics.loadRatio.value(),
                160.0 / 300.0),
        "plant A derived metrics are correct");

    expect(
        plantB != nullptr
            && plantB->metrics.totalUnits == 2
            && plantB->metrics.badQualityUnits == 1
            && plantB->metrics.noDataUnits == 1
            && !plantB->metrics.averageFrequencyHz.has_value()
            && !plantB->metrics.loadRatio.has_value(),
        "plant B unavailable data is excluded");

    const auto& grid = result.grids.front();

    expect(
        grid.plantCount == 2
            && grid.metrics.totalUnits == 6
            && grid.metrics.enabledUnits == 5
            && grid.metrics.freshUnits == 2
            && grid.metrics.staleUnits == 1
            && grid.metrics.badQualityUnits == 1
            && grid.metrics.noDataUnits == 1,
        "grid state counters are correct");

    expect(
        approximatelyEqual(
            grid.metrics.enabledRatedCapacityMw,
            1150.0)
            && approximatelyEqual(
                grid.metrics.onlineRatedCapacityMw,
                300.0)
            && approximatelyEqual(
                grid.metrics.activePowerMw,
                160.0),
        "grid power metrics are correct");
}

void testEmptyInputAndOptions()
{
    using namespace std::chrono;

    pfr::compute::RealtimeAggregator aggregator;

    const auto now =
        pfr::domain::TimePoint{seconds(100)};

    const auto empty = aggregator.aggregate({}, now);

    expect(
        empty.plants.empty() && empty.grids.empty(),
        "empty input produces empty aggregation");

    bool invalidOptionsRejected = false;

    try
    {
        pfr::compute::RealtimeAggregationOptions options;
        options.staleAfter = milliseconds(0);

        pfr::compute::RealtimeAggregator invalid(options);
        static_cast<void>(invalid);
    }
    catch (const std::invalid_argument&)
    {
        invalidOptionsRejected = true;
    }

    expect(
        invalidOptionsRejected,
        "non-positive stale threshold is rejected");
}

void testMultipleGridsRemainIndependent()
{
    using namespace std::chrono;

    const auto now =
        pfr::domain::TimePoint{seconds(100)};

    pfr::compute::RealtimeAggregator aggregator;

    auto east = makeSnapshot(
        makeUnit("shared-plant", "unit-1", 100.0),
        makeSample(
            "shared-plant",
            "unit-1",
            now - seconds(1),
            40.0,
            50.0,
            45.0,
            pfr::domain::DataQuality::Good));

    auto west = makeSnapshot(
        makeUnit("shared-plant", "unit-1", 200.0),
        makeSample(
            "shared-plant",
            "unit-1",
            now - seconds(1),
            120.0,
            50.0,
            125.0,
            pfr::domain::DataQuality::Good));

    west.unit.gridId = "grid-west";
    west.latest->gridId = "grid-west";

    const std::vector<
        pfr::compute::UnitRealtimeSnapshot
    > units{east, west};

    const auto result =
        aggregator.aggregate(units, now);

    const auto* eastGrid =
        findGrid(result, "grid-east");

    const auto* westGrid =
        findGrid(result, "grid-west");

    expect(
        result.grids.size() == 2
            && result.plants.size() == 2,
        "same plant id in different grids stays independent");

    expect(
        eastGrid != nullptr
            && eastGrid->metrics.totalUnits == 1
            && approximatelyEqual(
                eastGrid->metrics.activePowerMw,
                40.0),
        "east grid contains only east unit data");

    expect(
        westGrid != nullptr
            && westGrid->metrics.totalUnits == 1
            && approximatelyEqual(
                westGrid->metrics.activePowerMw,
                120.0),
        "west grid contains only west unit data");
}

} // namespace

int main()
{
    testClassification();
    testPlantAndGridAggregation();
    testMultipleGridsRemainIndependent();
    testEmptyInputAndOptions();

    return failedTests == 0 ? 0 : 1;
}