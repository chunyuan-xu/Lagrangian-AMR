"""Compile and run the M1.1 local timestep reduction unit test."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path("C:/msys64/ucrt64/bin/g++.exe")
SOURCE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include "physics/timestep_reduction.h"

static double reduce(const std::array<double, 5>& values)
{
    double result = TimestepReduction::initial_local_minimum();
    for (double value : values) {
        result = TimestepReduction::accumulate_local_minimum(result, value);
    }
    return result;
}

int main()
{
    const std::array<double, 5> forward{{0.30, 0.12, 0.50, 0.08, 0.20}};
    const std::array<double, 5> reverse{{0.20, 0.08, 0.50, 0.12, 0.30}};
    assert(std::isinf(TimestepReduction::initial_local_minimum()));
    assert(reduce(forward) == 0.08);
    assert(reduce(reverse) == 0.08);
    assert(TimestepReduction::accumulate_local_minimum(0.08, 0.40) == 0.08);
    assert(TimestepReduction::accumulate_local_minimum(0.08, 0.04) == 0.04);
    return 0;
}
'''


def main():
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_timestep_") as directory:
        directory = Path(directory)
        source = directory / "test.cpp"
        executable = directory / "test.exe"
        source.write_text(SOURCE, encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join([
            "C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", environment.get("PATH", "")
        ])
        compile_result = subprocess.run(
            [str(COMPILER), "-std=c++14", "-Wall", "-Wextra", "-Isrc", str(source), "-o", str(executable)],
            cwd=ROOT, env=environment, capture_output=True, text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return 1
        run_result = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        if run_result.returncode != 0:
            return run_result.returncode
    print("PASS: local timestep minimum is order independent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
