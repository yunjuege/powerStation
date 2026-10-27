#include "pfr/compute/ShardedComputeEngine.hpp"

#include <functional>
#include <utility>

namespace pfr::compute {

ShardedComputeEngine::ShardedComputeEngine(
    std::size_t shardCount)
{
    const auto actualShardCount =
        shardCount == 0 ? std::size_t{1} : shardCount;

    shards_.reserve(actualShardCount);

    for (std::size_t index = 0;
         index < actualShardCount;
         ++index)
    {
        shards_.push_back(std::make_unique<Shard>());
    }
}

void ShardedComputeEngine::registerUnit(
    domain::Unit unit)
{
    const auto key = domain::makeUnitKey(
        unit.gridId,
        unit.plantId,
        unit.id);

    auto processor =
        std::make_shared<UnitRealtimeProcessor>(
            std::move(unit));

    auto& shard = *shards_.at(shardIndex(key));

    std::lock_guard<std::mutex> lock(shard.mutex);

    const auto [iterator, inserted] =
        shard.processors.try_emplace(
            key,
            std::move(processor));

    static_cast<void>(iterator);

    if (inserted)
    {
        unitCount_.fetch_add(
            1,
            std::memory_order_relaxed);
    }
}

SampleDecision ShardedComputeEngine::ingest(
    const domain::Sample& sample)
{
    if (!domain::isValid(sample))
    {
        return SampleDecision::Invalid;
    }

    const auto key = domain::makeUnitKey(
        sample.gridId,
        sample.plantId,
        sample.unitId);

    const auto processor = findProcessor(key);

    if (!processor)
    {
        return SampleDecision::UnknownUnit;
    }

    return processor->ingest(sample);
}

std::optional<UnitRealtimeSnapshot>
ShardedComputeEngine::snapshotUnit(
    const std::string& gridId,
    const std::string& plantId,
    const std::string& unitId) const
{
    const auto key = domain::makeUnitKey(
        gridId,
        plantId,
        unitId);

    const auto processor = findProcessor(key);

    if (!processor)
    {
        return std::nullopt;
    }

    return processor->snapshot();
}

std::vector<UnitRealtimeSnapshot>
ShardedComputeEngine::snapshotAll() const
{
    std::vector<
        std::shared_ptr<UnitRealtimeProcessor>
    > processors;

    for (const auto& shardPointer : shards_)
    {
        const auto& shard = *shardPointer;

        {
            std::lock_guard<std::mutex> lock(
                shard.mutex);

            for (const auto& [key, processor]
                 : shard.processors)
            {
                static_cast<void>(key);
                processors.push_back(processor);
            }
        }
    }

    std::vector<UnitRealtimeSnapshot> result;
    result.reserve(processors.size());

    for (const auto& processor : processors)
    {
        result.push_back(processor->snapshot());
    }

    return result;
}

std::size_t ShardedComputeEngine::unitCount() const noexcept
{
    return unitCount_.load(
        std::memory_order_relaxed);
}

std::size_t ShardedComputeEngine::shardCount() const noexcept
{
    return shards_.size();
}

std::size_t ShardedComputeEngine::shardIndex(
    const std::string& unitKey) const noexcept
{
    return std::hash<std::string>{}(unitKey)
        % shards_.size();
}

std::shared_ptr<UnitRealtimeProcessor>
ShardedComputeEngine::findProcessor(
    const std::string& unitKey) const
{
    const auto& shard =
        *shards_.at(shardIndex(unitKey));

    std::lock_guard<std::mutex> lock(
        shard.mutex);

    const auto iterator =
        shard.processors.find(unitKey);

    if (iterator == shard.processors.end())
    {
        return {};
    }

    return iterator->second;
}

} // namespace pfr::compute