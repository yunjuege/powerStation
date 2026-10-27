#include "pfr/compute/ShardedComputeEngine.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

pfr::domain::Unit makeUnit(
    const std::string& plantId,
    const std::string& unitId)
{
    return {
        unitId,
        "grid-east",
        plantId,
        "Unit " + unitId,
        350.0,
        0.1,
        true
    };
}

pfr::domain::Sample makeSample(
    const std::string& plantId,
    const std::string& unitId,
    std::uint64_t sequence)
{
    const auto collectedAt =
        pfr::domain::TimePoint{
            std::chrono::seconds(1)}
        + std::chrono::milliseconds(
            static_cast<std::int64_t>(
                sequence * 100));

    pfr::domain::Sample result;

    result.gridId = "grid-east";
    result.plantId = plantId;
    result.unitId = unitId;

    result.collectedAt = collectedAt;
    result.receivedAt =
        collectedAt + std::chrono::milliseconds(5);

    result.activePowerMw =
        300.0 + static_cast<double>(sequence);

    result.frequencyHz = 49.98;
    result.agcCommandMw = 305.0;
    result.primaryFrequencyEnabled = true;
    result.quality = pfr::domain::DataQuality::Good;
    result.sequence = sequence;
    result.source = "modbus-tcp:channel-1";

    return result;
}

void testRegistrationAndRouting()
{
    using pfr::compute::SampleDecision;

    pfr::compute::ShardedComputeEngine fallbackEngine(0);

    expect(
        fallbackEngine.shardCount() == 1,
        "zero shard count falls back to one");

    pfr::compute::ShardedComputeEngine engine(4);

    engine.registerUnit(
        makeUnit("plant-a", "unit-1"));

    engine.registerUnit(
        makeUnit("plant-a", "unit-2"));

    engine.registerUnit(
        makeUnit("plant-b", "unit-1"));

    expect(
        engine.shardCount() == 4,
        "engine creates requested shard count");

    expect(
        engine.unitCount() == 3,
        "engine registers units from multiple plants");

    expect(
        engine.ingest(
            makeSample("plant-a", "unit-1", 1))
            == SampleDecision::Accepted,
        "sample is routed to plant A unit 1");

    engine.registerUnit(
        makeUnit("plant-a", "unit-1"));

    const auto afterDuplicateRegistration =
        engine.snapshotUnit(
            "grid-east",
            "plant-a",
            "unit-1");

    expect(
        engine.unitCount() == 3,
        "duplicate registration does not increase unit count");

    expect(
        afterDuplicateRegistration.has_value()
            && afterDuplicateRegistration->accepted == 1
            && afterDuplicateRegistration->latest.has_value(),
        "duplicate registration preserves realtime state");

    expect(
        engine.ingest(
            makeSample("plant-b", "unit-1", 1))
            == SampleDecision::Accepted,
        "same unit id in another plant is independent");

    const auto plantAUnit = engine.snapshotUnit(
        "grid-east",
        "plant-a",
        "unit-1");

    const auto plantBUnit = engine.snapshotUnit(
        "grid-east",
        "plant-b",
        "unit-1");

    expect(
        plantAUnit.has_value()
            && plantAUnit->accepted == 1,
        "plant A snapshot is available");

    expect(
        plantBUnit.has_value()
            && plantBUnit->accepted == 1,
        "plant B snapshot is available");

    expect(
        engine.snapshotAll().size() == 3,
        "full snapshot contains every registered unit");
}

void testRejectedSamples()
{
    using pfr::compute::SampleDecision;

    pfr::compute::ShardedComputeEngine engine(2);

    engine.registerUnit(
        makeUnit("plant-a", "unit-1"));

    expect(
        engine.ingest(
            makeSample("plant-x", "unit-x", 1))
            == SampleDecision::UnknownUnit,
        "unknown unit sample is rejected");

    auto invalid = makeSample(
        "plant-a",
        "unit-1",
        1);

    invalid.source.clear();

    expect(
        engine.ingest(invalid)
            == SampleDecision::Invalid,
        "invalid sample is rejected before routing");

    const auto first = makeSample(
        "plant-a",
        "unit-1",
        1);

    expect(
        engine.ingest(first)
            == SampleDecision::Accepted,
        "first registered sample is accepted");

    expect(
        engine.ingest(first)
            == SampleDecision::Duplicate,
        "duplicate result passes through engine");

    expect(
        engine.ingest(
            makeSample("plant-a", "unit-1", 0))
            == SampleDecision::OutOfOrder,
        "out-of-order result passes through engine");
}

void testConcurrentUnits()
{
    using pfr::compute::SampleDecision;

    constexpr int unitTotal = 8;
    constexpr std::uint64_t samplesPerUnit = 500;

    pfr::compute::ShardedComputeEngine engine(4);

    for (int unitIndex = 0;
         unitIndex < unitTotal;
         ++unitIndex)
    {
        engine.registerUnit(
            makeUnit(
                "plant-a",
                "unit-" + std::to_string(unitIndex)));
    }

    std::atomic<bool> writersValid{true};
    std::atomic<bool> readersValid{true};

    std::vector<std::thread> writers;

    for (int unitIndex = 0;
         unitIndex < unitTotal;
         ++unitIndex)
    {
        writers.emplace_back(
            [&engine, unitIndex, &writersValid]()
            {
                const auto unitId =
                    "unit-" + std::to_string(unitIndex);

                for (std::uint64_t sequence = 1;
                     sequence <= samplesPerUnit;
                     ++sequence)
                {
                    if (engine.ingest(
                            makeSample(
                                "plant-a",
                                unitId,
                                sequence))
                        != SampleDecision::Accepted)
                    {
                        writersValid = false;
                    }
                }
            });
    }

    std::vector<std::thread> readers;

    for (int readerIndex = 0;
         readerIndex < 4;
         ++readerIndex)
    {
        readers.emplace_back(
            [&engine, &readersValid]()
            {
                for (int read = 0; read < 200; ++read)
                {
                    const auto current =
                        engine.snapshotAll();

                    if (current.size() != unitTotal)
                    {
                        readersValid = false;
                    }
                }
            });
    }

    for (auto& writer : writers)
    {
        writer.join();
    }

    for (auto& reader : readers)
    {
        reader.join();
    }

    bool finalStatesValid = true;

    for (int unitIndex = 0;
         unitIndex < unitTotal;
         ++unitIndex)
    {
        const auto current = engine.snapshotUnit(
            "grid-east",
            "plant-a",
            "unit-" + std::to_string(unitIndex));

        if (!current.has_value()
            || current->accepted != samplesPerUnit
            || !current->latest.has_value()
            || current->latest->sequence != samplesPerUnit)
        {
            finalStatesValid = false;
        }
    }

    expect(
        writersValid
            && readersValid
            && finalStatesValid,
        "multiple units ingest and snapshot concurrently");
}

} // namespace

int main()
{
    testRegistrationAndRouting();
    testRejectedSamples();
    testConcurrentUnits();

    return failedTests == 0 ? 0 : 1;
}