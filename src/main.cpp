#include "pfr/domain/Types.hpp"

#include <iostream>

int main()
{
    const pfr::domain::Grid grid{
        "grid-east",
        "East Regional Grid",
        true
    };

    const pfr::domain::Plant plant{
        "plant-001",
        grid.id,
        "Demo Power Plant",
        true
    };

    const pfr::domain::Unit unit{
        "unit-01",
        grid.id,
        plant.id,
        "Unit 01",
        350.0,
        0.1,
        true
    };

    if (!pfr::domain::isValid(unit))
    {
        std::cerr << "Invalid unit configuration\n";
        return 1;
    }

    std::cout
        << "PFR Grid Terminal started\n"
        << "Grid: " << grid.name << '\n'
        << "Plant: " << plant.name << '\n'
        << "Unit: " << unit.name << '\n'
        << "Unit key: "
        << pfr::domain::makeUnitKey(grid.id, plant.id, unit.id)
        << '\n';

    return 0;
}