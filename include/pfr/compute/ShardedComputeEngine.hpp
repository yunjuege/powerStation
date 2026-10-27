#pragma once

#include "pfr/compute/UnitRealtimeProcessor.hpp"

#include <atomic>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pfr::compute {

class ShardedComputeEngine
{
public:
    explicit ShardedComputeEngine(
        std::size_t shardCount);

    void registerUnit(domain::Unit unit);

    [[nodiscard]] SampleDecision ingest(
        const domain::Sample& sample);

    [[nodiscard]] std::optional<UnitRealtimeSnapshot>
    snapshotUnit(
        const std::string& gridId,
        const std::string& plantId,
        const std::string& unitId) const;

    [[nodiscard]] std::vector<UnitRealtimeSnapshot>
    snapshotAll() const;

    [[nodiscard]] std::size_t unitCount() const noexcept;
    [[nodiscard]] std::size_t shardCount() const noexcept;

private:
    struct Shard
    {
        mutable std::mutex mutex;

        std::map<
            std::string,
            std::shared_ptr<UnitRealtimeProcessor>
        > processors;
    };

    [[nodiscard]] std::size_t shardIndex(
        const std::string& unitKey) const noexcept;

    [[nodiscard]] std::shared_ptr<UnitRealtimeProcessor>
    findProcessor(
        const std::string& unitKey) const;

    std::vector<std::unique_ptr<Shard>> shards_;
    std::atomic<std::size_t> unitCount_{0};
};

} // namespace pfr::compute