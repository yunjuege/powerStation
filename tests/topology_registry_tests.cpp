#include "pfr/domain/TopologyRegistry.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>
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
    const std::string& id,
    const std::string& plantId)
{
    return {
        id,
        "grid-east",
        plantId,
        "Unit " + id,
        350.0,
        0.1,
        true
    };
}

void testHierarchy()
{
    pfr::domain::TopologyRegistry registry;

    registry.upsertGrid({
        "grid-east",
        "East Regional Grid",
        true
    });

    registry.upsertPlant({
        "plant-a",
        "grid-east",
        "Plant A",
        true
    });

    registry.upsertPlant({
        "plant-b",
        "grid-east",
        "Plant B",
        true
    });

    registry.upsertUnit(makeUnit("unit-1", "plant-a"));
    registry.upsertUnit(makeUnit("unit-2", "plant-a"));
    registry.upsertUnit(makeUnit("unit-1", "plant-b"));

    expect(
        registry.listPlants("grid-east").size() == 2,
        "grid contains two plants");

    expect(
        registry.listUnits("grid-east", "plant-a").size() == 2,
        "plant A contains two units");

    expect(
        registry.listUnits("grid-east", "plant-b").size() == 1,
        "plant B contains one unit");

    const auto found = registry.findUnit(
        "grid-east",
        "plant-a",
        "unit-1");

    expect(
        found.has_value()
            && found->name == "Unit unit-1",
        "unit can be found by complete identity");

    expect(
        registry.snapshot().units.size() == 3,
        "snapshot contains all units");
}

void testOrphanProtection()
{
    pfr::domain::TopologyRegistry registry;

    registry.upsertGrid({
        "grid-east",
        "East Regional Grid",
        true
    });

    bool rejected = false;

    try
    {
        registry.upsertUnit(
            makeUnit("unit-1", "missing-plant"));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }

    expect(
        rejected,
        "unit cannot reference an unknown plant");
}

void testConcurrentSnapshots()
{
    pfr::domain::TopologyRegistry registry;

    registry.upsertGrid({
        "grid-east",
        "East Regional Grid",
        true
    });

    registry.upsertPlant({
        "plant-a",
        "grid-east",
        "Plant A",
        true
    });

    std::atomic<bool> snapshotsValid{true};

    std::thread writer([&registry]()
    {
        for (int index = 0; index < 100; ++index)
        {
            registry.upsertUnit(
                makeUnit(
                    "unit-" + std::to_string(index),
                    "plant-a"));
        }
    });

    std::vector<std::thread> readers;

    for (int index = 0; index < 4; ++index)
    {
        readers.emplace_back(
            [&registry, &snapshotsValid]()
            {
                for (int read = 0; read < 200; ++read)
                {
                    const auto current = registry.snapshot();

                    if (current.grids.size() != 1
                        || current.plants.size() != 1
                        || current.units.size() > 100)
                    {
                        snapshotsValid = false;
                    }
                }
            });
    }

    writer.join();

    for (auto& reader : readers)
    {
        reader.join();
    }

    expect(
        snapshotsValid
            && registry.snapshot().units.size() == 100,
        "concurrent readers obtain valid snapshots");
}

} // namespace

int main()
{
    testHierarchy();
    testOrphanProtection();
    testConcurrentSnapshots();

    return failedTests == 0 ? 0 : 1;
}