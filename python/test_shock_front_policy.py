"""Compile and run the shock-front geometry policy unit test."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path("C:/msys64/ucrt64/bin/g++.exe")
SOURCE = r'''
#include <array>
#include <cassert>
#include <cmath>
#include "amr/shock_front_policy.h"

int main()
{
    const ShockFrontPolicy::Trajectory legacy{1.0 / 3.0, 1.0};
    assert(std::fabs(ShockFrontPolicy::radius_at_time(0.5, legacy) - 1.0 / 6.0) < 1e-14);

    const ShockFrontPolicy::Trajectory sedov{0.83, 0.5};
    assert(std::fabs(ShockFrontPolicy::sedov_radius(0.25, 0.83) - 0.415) < 1e-14);
    assert(std::fabs(ShockFrontPolicy::sedov_radius(1.0, 0.83) - 0.83) < 1e-14);
    assert(std::fabs(ShockFrontPolicy::radius_at_time(0.0, sedov)) < 1e-14);
    assert(std::fabs(ShockFrontPolicy::radius_at_time(0.25, sedov) - 0.415) < 1e-14);
    assert(std::fabs(ShockFrontPolicy::radius_at_time(1.0, sedov) - 0.83) < 1e-14);

    const std::array<std::array<double, 2>, 4> crossing{{
        {{0.55, 0.00}}, {{0.75, 0.00}}, {{0.75, 0.20}}, {{0.55, 0.20}}
    }};
    const std::array<std::array<double, 2>, 4> inside{{
        {{0.10, 0.00}}, {{0.20, 0.00}}, {{0.20, 0.10}}, {{0.10, 0.10}}
    }};
    const std::array<std::array<double, 2>, 4> outside{{
        {{1.10, 0.00}}, {{1.20, 0.00}}, {{1.20, 0.10}}, {{1.10, 0.10}}
    }};

    const auto crossing_bounds = ShockFrontPolicy::radial_bounds(crossing);
    const auto inside_bounds = ShockFrontPolicy::radial_bounds(inside);
    const auto outside_bounds = ShockFrontPolicy::radial_bounds(outside);

    assert(ShockFrontPolicy::intersects_radial_band(crossing_bounds, 0.83, 0.22));
    assert(!ShockFrontPolicy::intersects_radial_band(inside_bounds, 0.83, 0.22));
    assert(!ShockFrontPolicy::intersects_radial_band(outside_bounds, 0.83, 0.22));
    return 0;
}
'''


def main() -> int:
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_shock_front_") as directory:
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
            return compile_result.returncode
        return subprocess.run([str(executable)], cwd=ROOT, env=environment).returncode


if __name__ == "__main__":
    raise SystemExit(main())
