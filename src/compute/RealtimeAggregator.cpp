#include "pfr/compute/RealtimeAggregator.hpp"

#include <map>
#include <stdexcept>
#include <utility>

namespace {

struct MutableMetrics
{
    pfr::compute::RealtimeAggregateMetrics metrics;
    double frequencySumHz{0.0};
};

void accumulate(
    MutableMetrics& target,
    const pfr::compute::UnitRealtimeSnapshot& unitSnapshot,
    pfr::compute::UnitRealtimeState state)
{
    using pfr::compute::UnitRealtimeState;

    auto& metrics = target.metrics;
    ++metrics.totalUnits;

    if (state == UnitRealtimeState::Disabled)
    {
        ++metrics.disabledUnits;
        return;
    }

    ++metrics.enabledUnits;
    metrics.enabledRatedCapacityMw +=
        unitSnapshot.unit.ratedCapacityMw;

    switch (state)
    {
    case UnitRealtimeState::Disabled:
        return;

    case UnitRealtimeState::NoData:
        ++metrics.noDataUnits;
        return;

    case UnitRealtimeState::Stale:
        ++metrics.staleUnits;
        return;

    case UnitRealtimeState::BadQuality:
        ++metrics.badQualityUnits;
        return;

    case UnitRealtimeState::Fresh:
        break;
    }

    ++metrics.freshUnits;

    metrics.onlineRatedCapacityMw +=
        unitSnapshot.unit.ratedCapacityMw;

    const auto& sample = unitSnapshot.latest.value();

    metrics.activePowerMw += sample.activePowerMw;
    metrics.agcCommandMw += sample.agcCommandMw;
    target.frequencySumHz += sample.frequencyHz;

    if (sample.quality == pfr::domain::DataQuality::Suspect)
    {
        ++metrics.suspectUnits;
    }
}

pfr::compute::RealtimeAggregateMetrics finalize(
    const MutableMetrics& source)
{
    auto result = source.metrics;

    if (result.freshUnits > 0)
    {
        result.averageFrequencyHz =
            source.frequencySumHz
            / static_cast<double>(result.freshUnits);
    }

    if (result.onlineRatedCapacityMw > 0.0)
    {
        result.loadRatio =
            result.activePowerMw
            / result.onlineRatedCapacityMw;
    }

    return result;
}

} // namespace

namespace pfr::compute {

RealtimeAggregator::RealtimeAggregator(
    RealtimeAggregationOptions options)
    : options_(options)
{
    if (options_.staleAfter.count() <= 0)
    {
        throw std::invalid_argument(
            "staleAfter must be greater than zero");
    }
}

UnitRealtimeState RealtimeAggregator::classify(
    const UnitRealtimeSnapshot& unitSnapshot,
    domain::TimePoint generatedAt) const noexcept
{
    if (!unitSnapshot.unit.enabled)
    {
        return UnitRealtimeState::Disabled;
    }

    if (!unitSnapshot.latest.has_value())
    {
        return UnitRealtimeState::NoData;
    }

    const auto& sample = unitSnapshot.latest.value();

    if (sample.quality == domain::DataQuality::Bad
        || sample.quality == domain::DataQuality::Missing)
    {
        return UnitRealtimeState::BadQuality;
    }

    if (sample.receivedAt > generatedAt)
    {
        return UnitRealtimeState::Stale;
    }

    if (generatedAt - sample.receivedAt
        > options_.staleAfter)
    {
        return UnitRealtimeState::Stale;
    }

    return UnitRealtimeState::Fresh;
}

RealtimeAggregationSnapshot RealtimeAggregator::aggregate(
    const std::vector<UnitRealtimeSnapshot>& unitSnapshots,
    domain::TimePoint generatedAt) const
{
    using PlantKey =
        std::pair<std::string, std::string>;

    std::map<PlantKey, MutableMetrics> plantMetrics;
    std::map<std::string, MutableMetrics> gridMetrics;

    for (const auto& unitSnapshot : unitSnapshots)
    {
        const auto state =
            classify(unitSnapshot, generatedAt);

        const PlantKey plantKey{
            unitSnapshot.unit.gridId,
            unitSnapshot.unit.plantId};

        accumulate(
            plantMetrics[plantKey],
            unitSnapshot,
            state);

        accumulate(
            gridMetrics[unitSnapshot.unit.gridId],
            unitSnapshot,
            state);
    }

    std::map<std::string, std::size_t> plantCounts;

    for (const auto& [plantKey, metrics] : plantMetrics)
    {
        static_cast<void>(metrics);
        ++plantCounts[plantKey.first];
    }

    RealtimeAggregationSnapshot result;
    result.generatedAt = generatedAt;

    result.plants.reserve(plantMetrics.size());

    for (const auto& [plantKey, metrics] : plantMetrics)
    {
        result.plants.push_back({
            plantKey.first,
            plantKey.second,
            finalize(metrics)});
    }

    result.grids.reserve(gridMetrics.size());

    for (const auto& [gridId, metrics] : gridMetrics)
    {
        result.grids.push_back({
            gridId,
            plantCounts[gridId],
            finalize(metrics)});
    }

    return result;
}

} // namespace pfr::compute