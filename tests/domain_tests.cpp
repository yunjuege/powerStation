#include "pfr/domain/Types.hpp"

#include <iostream>
#include <string>

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

} // namespace

int main()
{
    pfr::domain::Grid grid{
        "grid-east",
        "East Regional Grid",
        true
    };

    expect(
        pfr::domain::isValid(grid),
        "valid grid is accepted");

    pfr::domain::Unit unit{
        "unit-01",
        "grid-east",
        "plant-001",
        "Unit 01",
        350.0,
        0.1,
        true
    };

    expect(
        pfr::domain::isValid(unit),
        "valid unit is accepted");

    unit.ratedCapacityMw = 0.0;

    expect(
        !pfr::domain::isValid(unit),
        "zero capacity unit is rejected");

    expect(
        pfr::domain::makeUnitKey(
            "grid-east",
            "plant-001",
            "unit-01")
            == "grid-east/plant-001/unit-01",
        "unit key contains complete hierarchy");

    return failedTests == 0 ? 0 : 1;
}