#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace pfr::domain {

using TimePoint = std::chrono::system_clock::time_point;

enum class DataQuality : std::uint8_t
{
    Good,
    Suspect,
    Bad,
    Missing
};

struct Grid
{
    std::string id;
    std::string name;
    bool enabled{true};
};

struct Plant
{
    std::string id;
    std::string gridId;
    std::string name;
    bool enabled{true};
};

struct Unit
{
    std::string id;
    std::string gridId;
    std::string plantId;
    std::string name;
    double ratedCapacityMw{0.0};
    double samplingIntervalSeconds{1.0};
    bool enabled{true};
};

struct Sample
{
    std::string gridId;
    std::string plantId;
    std::string unitId;

    TimePoint collectedAt{};
    TimePoint receivedAt{};

    double activePowerMw{0.0};
    double frequencyHz{50.0};
    double agcCommandMw{0.0};

    bool primaryFrequencyEnabled{false};
    bool agcSteady{false};

    DataQuality quality{DataQuality::Good};
    std::uint64_t sequence{0};
    std::string source;
};

[[nodiscard]] bool isValid(const Grid& grid) noexcept;
[[nodiscard]] bool isValid(const Plant& plant) noexcept;
[[nodiscard]] bool isValid(const Unit& unit) noexcept;
[[nodiscard]] bool isValid(const Sample& sample) noexcept;

[[nodiscard]] std::string makeUnitKey(
    const std::string& gridId,
    const std::string& plantId,
    const std::string& unitId);

} // namespace pfr::domain