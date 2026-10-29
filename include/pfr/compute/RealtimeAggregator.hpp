#pragma once

#include "pfr/compute/UnitRealtimeProcessor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pfr::compute {

enum class UnitRealtimeState : std::uint8_t
{
    Disabled,
    NoData,
    Fresh,
    Stale,
    BadQuality
};

struct RealtimeAggregationOptions
{
    std::chrono::milliseconds staleAfter{5000};
};

struct RealtimeAggregateMetrics
{
    std::size_t totalUnits{0};
    std::size_t enabledUnits{0};
    std::size_t disabledUnits{0};

    std::size_t freshUnits{0};
    std::size_t staleUnits{0};
    std::size_t noDataUnits{0};
    std::size_t badQualityUnits{0};
    std::size_t suspectUnits{0};

    double enabledRatedCapacityMw{0.0};
    double onlineRatedCapacityMw{0.0};

    double activePowerMw{0.0};
    double agcCommandMw{0.0};

    std::optional<double> averageFrequencyHz;
    std::optional<double> loadRatio;
};

struct PlantRealtimeSnapshot
{
    std::string gridId;
    std::string plantId;
    RealtimeAggregateMetrics metrics;
};

struct GridRealtimeSnapshot
{
    std::string gridId;
    std::size_t plantCount{0};
    RealtimeAggregateMetrics metrics;
};

struct RealtimeAggregationSnapshot
{
    domain::TimePoint generatedAt;
    std::vector<PlantRealtimeSnapshot> plants;
    std::vector<GridRealtimeSnapshot> grids;
};

class RealtimeAggregator
{
public:
    explicit RealtimeAggregator(
        RealtimeAggregationOptions options = {});

    [[nodiscard]] UnitRealtimeState classify(
        const UnitRealtimeSnapshot& unitSnapshot,
        domain::TimePoint generatedAt) const noexcept;

    [[nodiscard]] RealtimeAggregationSnapshot aggregate(
        const std::vector<UnitRealtimeSnapshot>& unitSnapshots,
        domain::TimePoint generatedAt) const;

private:
    RealtimeAggregationOptions options_;
};

} // namespace pfr::compute