"""Focused compile/run test for the external memory high-water tracker."""

import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

SOURCE = r"""
#include <cassert>
#include <cstring>
#include <cstdint>
#include <limits>
#include "diagnostics/memory_probe_observer.h"

#ifdef NDEBUG
#error "This focused test requires assertions to remain enabled"
#endif

void assert_bridge_rejected(Diagnostics::MemoryProbeContext context,
    GhostExchangeObservation observation,
    Diagnostics::MemoryHighWaterTracker &expected_tracker)
{
    const Diagnostics::MemoryHighWaterValues before = expected_tracker.values();
    Diagnostics::observe_memory_exchange(&context, observation);
    assert(context.failure);
    assert(context.coverage_bits == 0);
    assert(std::memcmp(&expected_tracker.values(), &before, sizeof(before)) == 0);

    int valid_step = 1;
    context.tracker = &expected_tracker;
    context.current_step = &valid_step;
    context.origin = Diagnostics::ExchangeOrigin::Initial;
    observation.local_leaves = 1;
    observation.logical_send_entries = 1;
    observation.payload_bytes = sizeof(quad_data_t);
    Diagnostics::observe_memory_exchange(&context, observation);
    assert(std::memcmp(&expected_tracker.values(), &before, sizeof(before)) == 0);
}

int main()
{
    Diagnostics::MemoryHighWaterTracker tracker;
    Diagnostics::MemoryProbeSample first = {1, 4, 10, 3, 2, 4, 512};
    assert(tracker.observe_completed_exchange(first, 100, 120));
    const Diagnostics::MemoryHighWaterValues &a = tracker.values();
    assert(a.completed_exchange_count == 1);
    assert(a.max_local_payload_bytes == 1000);
    assert(a.max_ghost_payload_buffer_bytes == 300);
    assert(a.cumulative_logical_send_payload_bytes == 400);
    assert(a.cumulative_logical_receive_payload_bytes == 300);
    assert(a.cumulative_estimated_send_payload_bytes == 480);
    assert(a.max_estimated_send_payload_bytes == 480);
    assert(a.max_estimated_receive_payload_bytes == 360);
    assert(a.max_p4est_reported_ghost_bytes == 512);

    Diagnostics::MemoryProbeSample second = {2, 5, 8, 5, 3, 2, 768};
    assert(tracker.observe_completed_exchange(second, 100, 120));
    const Diagnostics::MemoryHighWaterValues &b = tracker.values();
    assert(b.completed_exchange_count == 2);
    assert(b.max_local_leaves == 10);
    assert(b.max_ghost_leaves == 5);
    assert(b.max_ghost_payload_buffer_bytes == 500);
    assert(b.max_estimated_send_payload_bytes == 480);
    assert(b.max_estimated_receive_payload_bytes == 600);
    assert(b.cumulative_logical_send_payload_bytes == 600);
    assert(b.cumulative_logical_receive_payload_bytes == 800);

    const Diagnostics::MemoryHighWaterValues before = tracker.values();
    Diagnostics::MemoryProbeSample overflow = {
        3, 6, std::numeric_limits<std::uint64_t>::max(), 0, 0, 0, 0
    };
    assert(!tracker.observe_completed_exchange(overflow, 2, 2));
    assert(std::memcmp(&tracker.values(), &before, sizeof(before)) == 0);
    assert(!tracker.observe_completed_exchange(first, 0, 120));
    assert(!tracker.observe_completed_exchange(first, 100, 0));
    assert(std::memcmp(&tracker.values(), &before, sizeof(before)) == 0);

    Diagnostics::MemoryHighWaterTracker add_overflow_tracker;
    Diagnostics::MemoryProbeSample near_limit = {
        1, 1, 0, 0, 0, std::numeric_limits<std::uint64_t>::max() - 5, 0
    };
    assert(add_overflow_tracker.observe_completed_exchange(near_limit, 1, 1));
    const Diagnostics::MemoryHighWaterValues before_add_overflow =
        add_overflow_tracker.values();
    Diagnostics::MemoryProbeSample crosses_limit = {2, 2, 0, 0, 0, 6, 0};
    assert(!add_overflow_tracker.observe_completed_exchange(crosses_limit, 1, 1));
    assert(std::memcmp(&add_overflow_tracker.values(), &before_add_overflow,
        sizeof(before_add_overflow)) == 0);

    tracker.reset();
    Diagnostics::MemoryHighWaterValues zero = {};
    assert(std::memcmp(&tracker.values(), &zero, sizeof(zero)) == 0);

    Diagnostics::MemoryHighWaterTracker observed_tracker;
    int current_step = 9;
    Diagnostics::MemoryProbeContext context = {
        &observed_tracker,
        &current_step,
        Diagnostics::ExchangeOrigin::Initial,
        false,
        0
    };
    GhostExchangeObservation observation = {
        1, 10, 3, 2, 4, sizeof(quad_data_t), 512
    };
    Diagnostics::observe_memory_exchange(&context, observation);
    assert(!context.failure);
    assert(context.coverage_bits == 1);
    assert(observed_tracker.values().completed_exchange_count == 1);
    assert(observed_tracker.values().max_step == 9);
    assert(observed_tracker.values().max_estimated_local_payload_bytes ==
        10 * Diagnostics::kProjectedDualLayoutPayloadBytes);

    current_step = 10;
    context.origin = Diagnostics::ExchangeOrigin::Rebuild;
    observation.generation = 2;
    Diagnostics::observe_memory_exchange(&context, observation);
    assert(!context.failure);
    assert(context.coverage_bits == 3);
    context.origin = Diagnostics::ExchangeOrigin::Ordinary;
    Diagnostics::observe_memory_exchange(&context, observation);
    assert(!context.failure);
    assert(context.coverage_bits == 7);
    assert(observed_tracker.values().completed_exchange_count == 3);

    Diagnostics::MemoryHighWaterTracker negative_step_tracker;
    int bad_step = -1;
    Diagnostics::MemoryProbeContext negative_step = {
        &negative_step_tracker,
        &bad_step,
        Diagnostics::ExchangeOrigin::Initial,
        false,
        0
    };
    assert_bridge_rejected(negative_step, observation, negative_step_tracker);

    Diagnostics::MemoryHighWaterTracker bad_payload_tracker;
    bad_step = 1;
    Diagnostics::MemoryProbeContext bad_payload = {
        &bad_payload_tracker,
        &bad_step,
        Diagnostics::ExchangeOrigin::Ordinary,
        false,
        0
    };
    observation.payload_bytes = sizeof(quad_data_t) - 1;
    assert_bridge_rejected(bad_payload, observation, bad_payload_tracker);

    observation.payload_bytes = sizeof(quad_data_t);
    Diagnostics::MemoryHighWaterTracker null_tracker_target;
    Diagnostics::MemoryProbeContext null_tracker = {
        NULL, &bad_step, Diagnostics::ExchangeOrigin::Initial, false, 0
    };
    assert_bridge_rejected(null_tracker, observation, null_tracker_target);

    Diagnostics::MemoryHighWaterTracker null_step_tracker;
    Diagnostics::MemoryProbeContext null_step = {
        &null_step_tracker, NULL, Diagnostics::ExchangeOrigin::Initial, false, 0
    };
    assert_bridge_rejected(null_step, observation, null_step_tracker);

    Diagnostics::MemoryHighWaterTracker invalid_origin_tracker;
    Diagnostics::MemoryProbeContext invalid_origin = {
        &invalid_origin_tracker,
        &bad_step,
        static_cast<Diagnostics::ExchangeOrigin>(255),
        false,
        0
    };
    assert_bridge_rejected(invalid_origin, observation, invalid_origin_tracker);

    Diagnostics::MemoryHighWaterTracker bridge_overflow_tracker;
    Diagnostics::MemoryProbeContext bridge_overflow = {
        &bridge_overflow_tracker,
        &bad_step,
        Diagnostics::ExchangeOrigin::Ordinary,
        false,
        0
    };
    observation.local_leaves = std::numeric_limits<std::uint64_t>::max();
    assert_bridge_rejected(bridge_overflow, observation, bridge_overflow_tracker);
    return 0;
}
"""

NULL_CONTEXT_SOURCE = r"""
#include "diagnostics/memory_probe_observer.h"

#ifdef NDEBUG
#error "This focused test requires assertions to remain enabled"
#endif

int main()
{
    GhostExchangeObservation observation = {};
    Diagnostics::observe_memory_exchange(NULL, observation);
    return 0;
}
"""


def main():
    if not CXX.exists():
        raise SystemExit(f"compiler not found: {CXX}")
    with tempfile.TemporaryDirectory(prefix="memory-high-water-") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", environment.get("PATH", "")]
        )
        environment["TEMP"] = str(build)
        environment["TMP"] = str(build)
        environment["TMPDIR"] = str(build)
        compile_result = subprocess.run(
            [
                str(CXX),
                "-O2",
                "-g",
                "-Wall",
                "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return 1
        run_result = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        if run_result.returncode != 0:
            return run_result.returncode

        source.write_text(NULL_CONTEXT_SOURCE, encoding="ascii")
        death_compile = subprocess.run(
            [
                str(CXX),
                "-O2",
                "-g",
                "-Wall",
                "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
        )
        if death_compile.returncode != 0:
            print(death_compile.stdout + death_compile.stderr)
            return 1
        death_run = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        if death_run.returncode == 0:
            print("NULL memory observer context did not abort in debug build")
            return 1
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
