"""Focused gate for versioned per-rank memory high-water output + aggregation."""

import json
import os
import subprocess
import tempfile
from pathlib import Path

import aggregate_memory_high_water as aggregator


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

SOURCE = r"""
#include <cassert>
#include <cstdlib>
#include "diagnostics/memory_probe_output.h"

int main()
{
    int step = 4;
    Diagnostics::MemoryProbeState state(&step);
    Diagnostics::MemoryProbeContext &ctx = state.context;
    Diagnostics::MemoryProbeSample sample = {1, 4, 10, 3, 2, 4, 512};
    assert(ctx.tracker->observe_completed_exchange(
        sample, sizeof(quad_data_t), 7072));
    ctx.origin = Diagnostics::ExchangeOrigin::Rebuild;
    ctx.failure = false;
    ctx.coverage_bits = 7;
    assert(Diagnostics::write_memory_high_water_rank_output(2, 4, ctx));
    return 0;
}
"""


def main():
    if not CXX.exists():
        raise SystemExit(f"compiler not found: {CXX}")
    temp_root = ROOT / ".tmp"
    temp_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="memory-probe-output-", dir=temp_root) as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", environment.get("PATH", "")]
        )
        environment["LAGRANGIAN_MEMORY_HIGH_WATER_DIR"] = str(build)
        compile_result = subprocess.run(
            [
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source), "-o", str(executable),
            ],
            cwd=ROOT, env=environment, capture_output=True, text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return 1
        run = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        if run.returncode != 0:
            print("writer executable failed")
            return 1

        rank_path = build / "memory_high_water_rank_2.json"
        if not rank_path.exists():
            print("per-rank JSON not written")
            return 1
        payload = json.loads(rank_path.read_text(encoding="utf-8"))
        assert payload["schema"] == aggregator.SCHEMA_PER_RANK
        assert payload["rank"] == 2
        assert payload["size"] == 4
        assert payload["coverage_bits"] == 7
        assert payload["values"]["completed_exchange_count"] == 1
        assert payload["values"]["max_step"] == 4

        aggregate = aggregator.aggregate(build, expected_ranks=None,
                                         expected_size=4)
        assert aggregate["rank_count"] == 1
        assert aggregate["coverage_union_bits"] == 7
        assert aggregate["total_completed_exchange_count"] == 1
        assert aggregate["global_max"]["max_step"] == 4

        # Negative: missing coverage must be rejected.
        bad = dict(payload)
        bad["coverage_bits"] = 0
        (build / "bad.json").write_text(json.dumps(bad), encoding="utf-8")
        try:
            aggregator.validate_rank(2, bad, None, 4)
        except ValueError:
            pass
        else:
            raise AssertionError("coverage_bits=0 should be rejected")

    print("PASS: versioned per-rank memory high-water output + aggregation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())