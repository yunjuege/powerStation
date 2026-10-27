#include "pfr/domain/Types.hpp"

#include <cmath>

namespace pfr::domain {

bool isValid(const Grid& grid) noexcept
{
    return !grid.id.empty() && !grid.name.empty();
}

bool isValid(const Plant& plant) noexcept
{
    return !plant.id.empty()
        && !plant.gridId.empty()
        && !plant.name.empty();
}

bool isValid(const Unit& unit) noexcept
{
    return !unit.id.empty()
        && !unit.gridId.empty()
        && !unit.plantId.empty()
        && !unit.name.empty()
        && std::isfinite(unit.ratedCapacityMw)
        && unit.ratedCapacityMw > 0.0
        && std::isfinite(unit.samplingIntervalSeconds)
        && unit.samplingIntervalSeconds > 0.0;
}

bool isValid(const Sample& sample) noexcept
{
    return !sample.gridId.empty()
        && !sample.plantId.empty()
        && !sample.unitId.empty()
        && !sample.source.empty()
        && std::isfinite(sample.activePowerMw)
        && std::isfinite(sample.frequencyHz)
        && std::isfinite(sample.agcCommandMw)
        && sample.collectedAt.time_since_epoch().count() > 0
        && sample.receivedAt.time_since_epoch().count() > 0;
}

std::string makeUnitKey(
    const std::string& gridId,
    const std::string& plantId,
    const std::string& unitId)
{
    return gridId + '/' + plantId + '/' + unitId;
}

} // namespace pfr::domain