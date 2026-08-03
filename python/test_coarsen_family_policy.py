"""Compile and run the M1.2 coarsen family policy unit test."""

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
#include "amr/coarsen_family_policy.h"

using AMRCoarsenPolicy::ChildIndicator;
using AMRCoarsenPolicy::FamilyPolicy;
using AMRCoarsenPolicy::IndicatorMode;

static int count_permutations(
    std::array<ChildIndicator, 4> children,
    const FamilyPolicy& policy,
    bool expected)
{
    std::array<int, 4> order{{0, 1, 2, 3}};
    int count = 0;
    do {
        std::array<ChildIndicator, 4> permuted{{
            children[order[0]], children[order[1]],
            children[order[2]], children[order[3]]
        }};
        assert(AMRCoarsenPolicy::family_allows_coarsening(permuted, policy) == expected);
        ++count;
    } while (std::next_permutation(order.begin(), order.end()));
    return count;
}

int main()
{
    const FamilyPolicy gradient{IndicatorMode::Gradient, 5, 7, 0.8};
    const FamilyPolicy distance{IndicatorMode::DistanceFromShock, 5, 7, 0.8};

    std::array<ChildIndicator, 4> all_low{{
        {6, true, 0.1}, {6, true, 0.2}, {6, true, 0.3}, {6, true, 0.7}
    }};
    assert(count_permutations(all_low, gradient, true) == 24);

    auto one_high = all_low;
    one_high[2].value = 0.9;
    assert(count_permutations(one_high, gradient, true) == 24);

    std::array<ChildIndicator, 4> all_high{{
        {6, true, 0.8}, {6, true, 0.9}, {6, true, 1.0}, {6, true, 1.1}
    }};
    assert(count_permutations(all_high, gradient, false) == 24);

    auto one_forbidden = all_low;
    one_forbidden[3].coarsening_allowed = false;
    assert(count_permutations(one_forbidden, gradient, false) == 24);

    auto at_minimum = all_low;
    at_minimum[0].level = 5;
    assert(count_permutations(at_minimum, gradient, false) == 24);

    auto above_maximum = all_high;
    above_maximum[2].level = 8;
    assert(count_permutations(above_maximum, gradient, true) == 24);

    auto one_nan = all_high;
    one_nan[0].value = std::numeric_limits<double>::quiet_NaN();
    assert(count_permutations(one_nan, gradient, false) == 24);

    auto one_infinity = all_high;
    one_infinity[0].value = std::numeric_limits<double>::infinity();
    assert(count_permutations(one_infinity, gradient, false) == 24);

    std::array<ChildIndicator, 4> all_outside{{
        {6, true, 0.9}, {6, true, 1.0}, {6, true, 1.1}, {6, true, 1.2}
    }};
    assert(count_permutations(all_outside, distance, true) == 24);

    auto one_inside = all_outside;
    one_inside[2].value = 0.7;
    assert(count_permutations(one_inside, distance, true) == 24);

    std::array<ChildIndicator, 4> all_inside{{
        {6, true, 0.1}, {6, true, 0.2}, {6, true, 0.7}, {6, true, 0.8}
    }};
    assert(count_permutations(all_inside, distance, false) == 24);

    return 0;
}
'''


def main():
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_coarsen_") as directory:
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
    print("PASS: coarsen family policy is order independent across 24 permutations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
