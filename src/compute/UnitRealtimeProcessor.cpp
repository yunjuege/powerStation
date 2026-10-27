#include "pfr/compute/UnitRealtimeProcessor.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace pfr::compute {

UnitRealtimeProcessor::UnitRealtimeProcessor(
    domain::Unit unit,
    std::size_t recentIdentityCapacity)
    : unit_(std::move(unit)),
      recentIdentityCapacity_(
          recentIdentityCapacity == 0
              ? 1
              : recentIdentityCapacity)
{
    if (!domain::isValid(unit_))
    {
        throw std::invalid_argument(
            "UnitRealtimeProcessor requires a valid unit");
    }
}

SampleDecision UnitRealtimeProcessor::ingest(
    const domain::Sample& sample)
{
    if (!domain::isValid(sample))
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++invalid_;
        return SampleDecision::Invalid;
    }

    const auto identity = sampleIdentity(sample);

    std::lock_guard<std::mutex> lock(mutex_);

    if (!matchesUnit(sample))
    {
        ++identityMismatches_;
        return SampleDecision::IdentityMismatch;
    }

    if (recentIdentities_.find(identity)
        != recentIdentities_.end())
    {
        ++duplicates_;
        return SampleDecision::Duplicate;
    }

    if (latest_.has_value()
        && sample.collectedAt <= latest_->collectedAt)
    {
        ++outOfOrder_;
        return SampleDecision::OutOfOrder;
    }

    latest_ = sample;
    rememberIdentity(identity);
    ++accepted_;

    return SampleDecision::Accepted;
}

UnitRealtimeSnapshot UnitRealtimeProcessor::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    UnitRealtimeSnapshot result;

    result.unit = unit_;
    result.latest = latest_;
    result.accepted = accepted_;
    result.duplicates = duplicates_;
    result.outOfOrder = outOfOrder_;
    result.identityMismatches = identityMismatches_;
    result.invalid = invalid_;

    return result;
}

bool UnitRealtimeProcessor::matchesUnit(
    const domain::Sample& sample) const noexcept
{
    return sample.gridId == unit_.gridId
        && sample.plantId == unit_.plantId
        && sample.unitId == unit_.id;
}

std::string UnitRealtimeProcessor::sampleIdentity(
    const domain::Sample& sample)
{
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            sample.collectedAt.time_since_epoch())
            .count();

    return std::to_string(sample.sequence)
        + '@'
        + std::to_string(timestamp);
}

void UnitRealtimeProcessor::rememberIdentity(
    std::string identity)
{
    recentIdentities_.insert(identity);
    recentIdentityOrder_.push_back(std::move(identity));

    while (recentIdentityOrder_.size()
        > recentIdentityCapacity_)
    {
        recentIdentities_.erase(
            recentIdentityOrder_.front());

        recentIdentityOrder_.pop_front();
    }
}

} // namespace pfr::compute