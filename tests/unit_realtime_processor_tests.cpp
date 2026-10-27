#include "pfr/compute/UnitRealtimeProcessor.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
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

pfr::domain::Unit makeUnit()
{
    return {
        "unit-01",
        "grid-east",
        "plant-a",
        "Unit 01",
        350.0,
        0.1,
        true
    };
}

pfr::domain::Sample makeSample(
    std::uint64_t sequence,
    std::int64_t offsetMilliseconds)
{
    const auto collectedAt =
        pfr::domain::TimePoint{
            std::chrono::seconds(1)}
        + std::chrono::milliseconds(
            offsetMilliseconds);

    pfr::domain::Sample result;

    result.gridId = "grid-east";
    result.plantId = "plant-a";
    result.unitId = "unit-01";

    result.collectedAt = collectedAt;
    result.receivedAt =
        collectedAt + std::chrono::milliseconds(5);

    result.activePowerMw =
        300.0 + static_cast<double>(sequence);

    result.frequencyHz = 49.98;
    result.agcCommandMw = 305.0;
    result.primaryFrequencyEnabled = true;
    result.agcSteady = false;
    result.quality = pfr::domain::DataQuality::Good;
    result.sequence = sequence;
    result.source = "modbus-tcp:channel-1";

    return result;
}

void testSampleDecisions()
{
    using pfr::compute::SampleDecision;

    pfr::compute::UnitRealtimeProcessor processor(
        makeUnit(),
        8);

    const auto first = makeSample(1, 100);

    expect(
        processor.ingest(first)
            == SampleDecision::Accepted,
        "first valid sample is accepted");

    expect(
        processor.ingest(first)
            == SampleDecision::Duplicate,
        "duplicate sample is ignored");

    expect(
        processor.ingest(makeSample(2, 200))
            == SampleDecision::Accepted,
        "newer sample advances realtime state");

    expect(
        processor.ingest(makeSample(3, 150))
            == SampleDecision::OutOfOrder,
        "older sample does not overwrite realtime state");

    auto wrongUnit = makeSample(4, 300);
    wrongUnit.unitId = "unit-02";

    expect(
        processor.ingest(wrongUnit)
            == SampleDecision::IdentityMismatch,
        "sample cannot enter another unit processor");

    auto invalid = makeSample(5, 400);
    invalid.source.clear();

    expect(
        processor.ingest(invalid)
            == SampleDecision::Invalid,
        "invalid sample is rejected");

    const auto current = processor.snapshot();

    expect(
        current.accepted == 2
            && current.duplicates == 1
            && current.outOfOrder == 1
            && current.identityMismatches == 1
            && current.invalid == 1,
        "snapshot exposes ingestion counters");

    expect(
        current.latest.has_value()
            && current.latest->sequence == 2,
        "rejected samples do not overwrite latest state");
}

void testConcurrentSnapshots()
{
    pfr::compute::UnitRealtimeProcessor processor(
        makeUnit());

    std::atomic<bool> snapshotsValid{true};

    std::thread writer([&processor]()
    {
        for (std::uint64_t sequence = 1;
             sequence <= 1000;
             ++sequence)
        {
            static_cast<void>(
                processor.ingest(
                    makeSample(
                        sequence,
                        static_cast<std::int64_t>(
                            sequence * 100))));
        }
    });

    std::vector<std::thread> readers;

    for (int index = 0; index < 4; ++index)
    {
        readers.emplace_back(
            [&processor, &snapshotsValid]()
            {
                for (int read = 0; read < 500; ++read)
                {
                    const auto current =
                        processor.snapshot();

                    if (current.latest.has_value()
                        && current.accepted == 0)
                    {
                        snapshotsValid = false;
                    }

                    if (current.latest.has_value()
                        && current.latest->sequence
                            > current.accepted)
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

    const auto finalState = processor.snapshot();

    expect(
        snapshotsValid
            && finalState.accepted == 1000
            && finalState.latest.has_value()
            && finalState.latest->sequence == 1000,
        "concurrent readers obtain consistent snapshots");
}

} // namespace

int main()
{
    testSampleDecisions();
    testConcurrentSnapshots();

    return failedTests == 0 ? 0 : 1;
}