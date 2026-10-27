#pragma once

#include "pfr/domain/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>

namespace pfr::compute {

enum class SampleDecision : std::uint8_t
{
    Accepted,
    Duplicate,
    OutOfOrder,
    IdentityMismatch,
    Invalid,
    UnknownUnit
};

struct UnitRealtimeSnapshot
{
    domain::Unit unit;
    std::optional<domain::Sample> latest;

    std::uint64_t accepted{0};
    std::uint64_t duplicates{0};
    std::uint64_t outOfOrder{0};
    std::uint64_t identityMismatches{0};
    std::uint64_t invalid{0};
};

class UnitRealtimeProcessor
{
public:
    explicit UnitRealtimeProcessor(
        domain::Unit unit,
        std::size_t recentIdentityCapacity = 4096);

    [[nodiscard]] SampleDecision ingest(
        const domain::Sample& sample);

    [[nodiscard]] UnitRealtimeSnapshot snapshot() const;

private:
    [[nodiscard]] bool matchesUnit(
        const domain::Sample& sample) const noexcept;

    [[nodiscard]] static std::string sampleIdentity(
        const domain::Sample& sample);

    void rememberIdentity(std::string identity);

    mutable std::mutex mutex_;

    domain::Unit unit_;
    std::optional<domain::Sample> latest_;

    std::size_t recentIdentityCapacity_;

    std::deque<std::string> recentIdentityOrder_;
    std::unordered_set<std::string> recentIdentities_;

    std::uint64_t accepted_{0};
    std::uint64_t duplicates_{0};
    std::uint64_t outOfOrder_{0};
    std::uint64_t identityMismatches_{0};
    std::uint64_t invalid_{0};
};

} // namespace pfr::compute