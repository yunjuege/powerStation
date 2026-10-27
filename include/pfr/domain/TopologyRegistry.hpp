#pragma once

#include "pfr/domain/Types.hpp"

#include <map>
#include <optional>
#include <mutex>
#include <string>
#include <vector>

namespace pfr::domain {

struct TopologySnapshot
{
    std::vector<Grid> grids;
    std::vector<Plant> plants;
    std::vector<Unit> units;
};

class TopologyRegistry
{
public:
    void upsertGrid(Grid grid);
    void upsertPlant(Plant plant);
    void upsertUnit(Unit unit);

    [[nodiscard]] std::optional<Grid> findGrid(
        const std::string& gridId) const;

    [[nodiscard]] std::optional<Plant> findPlant(
        const std::string& gridId,
        const std::string& plantId) const;

    [[nodiscard]] std::optional<Unit> findUnit(
        const std::string& gridId,
        const std::string& plantId,
        const std::string& unitId) const;

    [[nodiscard]] std::vector<Plant> listPlants(
        const std::string& gridId) const;

    [[nodiscard]] std::vector<Unit> listUnits(
        const std::string& gridId,
        const std::string& plantId) const;

    [[nodiscard]] TopologySnapshot snapshot() const;

private:
    [[nodiscard]] static std::string makePlantKey(
        const std::string& gridId,
        const std::string& plantId);

    mutable std::mutex mutex_;

    std::map<std::string, Grid> grids_;
    std::map<std::string, Plant> plants_;
    std::map<std::string, Unit> units_;
};

} // namespace pfr::domain