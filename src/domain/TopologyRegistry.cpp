#include "pfr/domain/TopologyRegistry.hpp"

#include <mutex>
#include <stdexcept>
#include <utility>

namespace pfr::domain {

std::string TopologyRegistry::makePlantKey(
    const std::string& gridId,
    const std::string& plantId)
{
    return gridId + '/' + plantId;
}

void TopologyRegistry::upsertGrid(Grid grid)
{
    if (!isValid(grid))
    {
        throw std::invalid_argument("invalid grid");
    }

    const auto key = grid.id;

    std::lock_guard<std::mutex> lock(mutex_);
    grids_.insert_or_assign(key, std::move(grid));
}

void TopologyRegistry::upsertPlant(Plant plant)
{
    if (!isValid(plant))
    {
        throw std::invalid_argument("invalid plant");
    }

    const auto key = makePlantKey(plant.gridId, plant.id);

    std::lock_guard<std::mutex> lock(mutex_);

    if (grids_.find(plant.gridId) == grids_.end())
    {
        throw std::invalid_argument(
            "plant references unknown grid: " + plant.gridId);
    }

    plants_.insert_or_assign(key, std::move(plant));
}

void TopologyRegistry::upsertUnit(Unit unit)
{
    if (!isValid(unit))
    {
        throw std::invalid_argument("invalid unit");
    }

    const auto plantKey = makePlantKey(
        unit.gridId,
        unit.plantId);

    const auto key = makeUnitKey(
        unit.gridId,
        unit.plantId,
        unit.id);

    std::lock_guard<std::mutex> lock(mutex_);

    if (plants_.find(plantKey) == plants_.end())
    {
        throw std::invalid_argument(
            "unit references unknown plant: " + unit.plantId);
    }

    units_.insert_or_assign(key, std::move(unit));
}

std::optional<Grid> TopologyRegistry::findGrid(
    const std::string& gridId) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = grids_.find(gridId);

    if (it == grids_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

std::optional<Plant> TopologyRegistry::findPlant(
    const std::string& gridId,
    const std::string& plantId) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = plants_.find(
        makePlantKey(gridId, plantId));

    if (it == plants_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

std::optional<Unit> TopologyRegistry::findUnit(
    const std::string& gridId,
    const std::string& plantId,
    const std::string& unitId) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = units_.find(
        makeUnitKey(gridId, plantId, unitId));

    if (it == units_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

std::vector<Plant> TopologyRegistry::listPlants(
    const std::string& gridId) const
{
    std::vector<Plant> result;

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [key, plant] : plants_)
    {
        static_cast<void>(key);

        if (plant.gridId == gridId)
        {
            result.push_back(plant);
        }
    }

    return result;
}

std::vector<Unit> TopologyRegistry::listUnits(
    const std::string& gridId,
    const std::string& plantId) const
{
    std::vector<Unit> result;

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [key, unit] : units_)
    {
        static_cast<void>(key);

        if (unit.gridId == gridId
            && unit.plantId == plantId)
        {
            result.push_back(unit);
        }
    }

    return result;
}

TopologySnapshot TopologyRegistry::snapshot() const
{
    TopologySnapshot result;

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [key, grid] : grids_)
    {
        static_cast<void>(key);
        result.grids.push_back(grid);
    }

    for (const auto& [key, plant] : plants_)
    {
        static_cast<void>(key);
        result.plants.push_back(plant);
    }

    for (const auto& [key, unit] : units_)
    {
        static_cast<void>(key);
        result.units.push_back(unit);
    }

    return result;
}

} // namespace pfr::domain