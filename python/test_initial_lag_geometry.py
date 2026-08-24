"""Focused regression for deterministic initial lag geometry."""

import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "alg.h"
#include "init/initial_geometry.h"

#ifdef NDEBUG
#error "This focused test requires assertions to remain enabled"
#endif

void run_case(int problem)
{
    CDoubleVector coord_cur[CNDIM] = {
        CDoubleVector(0.0, 0.0), CDoubleVector(0.0, 0.25),
        CDoubleVector(0.25, 0.25), CDoubleVector(0.25, 0.0)
    };
    CDoubleVector coord_lag[CNDIM];
    CDoubleVector velocity_cur[CNDIM];
    CDoubleVector velocity_lag[CNDIM];
    for (int i = 0; i < CNDIM; ++i) {
        coord_lag[i] = CDoubleVector(1000.0 + i, -1000.0 - i);
        velocity_cur[i] = CDoubleVector(0.0, 0.0);
        velocity_lag[i] = CDoubleVector(0.0, 0.0);
    }

    double density_cur = 0.0, density_lag = 0.0;
    double volume = 0.0, mass = 0.0;
    CDoubleVector centroid_cur(0.0, 0.0);
    CDoubleVector centroid_lag(777.0, -888.0);
    CDoubleVector centroid_velocity_cur(0.0, 0.0);
    CDoubleVector centroid_velocity_lag(0.0, 0.0);
    double internal_energy_cur = 0.0, internal_energy_lag = 0.0;
    double pressure_cur = 0.0, pressure_lag = 0.0;
    double total_energy_cur = 0.0, total_energy_lag = 0.0;
    double sound_speed = 0.0, gamma = 0.0;
    int top = 0, bottom = 0, left = 0, right = 0;
    double top_value = 0.0, bottom_value = 0.0;
    double left_value = 0.0, right_value = 0.0;

    PhysicalAlg::InitCondition(problem, p4est_data_t::plane,
        0, 0, 0, 0, 4,
        coord_cur, coord_lag, velocity_cur, velocity_lag,
        density_cur, density_lag, volume, mass,
        centroid_cur, centroid_lag,
        centroid_velocity_cur, centroid_velocity_lag,
        internal_energy_cur, internal_energy_lag,
        pressure_cur, pressure_lag,
        total_energy_cur, total_energy_lag,
        sound_speed, gamma,
        top, bottom, left, right,
        top_value, bottom_value, left_value, right_value);

    for (int i = 0; i < CNDIM; ++i) {
        assert(coord_lag[i].x == coord_cur[i].x);
        assert(coord_lag[i].y == coord_cur[i].y);
    }
    assert(std::isfinite(centroid_cur.x));
    assert(std::isfinite(centroid_cur.y));
    assert(centroid_lag.x == centroid_cur.x);
    assert(centroid_lag.y == centroid_cur.y);
}

void test_corner_seed()
{
    CDoubleVector current[CNDIM];
    CDoubleVector lag[CNDIM];
    for (int i = 0; i < CNDIM; ++i) {
        current[i] = CDoubleVector(0.25 * i, -0.5 * i);
        lag[i] = CDoubleVector(1000.0 + i, -1000.0 - i);
    }
    InitialGeometry::seed_lag_corners(current, lag);
    for (int i = 0; i < CNDIM; ++i) {
        assert(lag[i].x == current[i].x);
        assert(lag[i].y == current[i].y);
    }
}

int main()
{
    test_corner_seed();
    run_case(ProblemNo::SedovPolar);
    run_case(ProblemNo::SedovCartesian);
    run_case(ProblemNo::Sedov1DCartesian);
    run_case(ProblemNo::NohPolar);
    run_case(ProblemNo::SodCartesian);
    run_case(ProblemNo::NohCartesian);
    run_case(ProblemNo::TriplePoint);
    run_case(ProblemNo::TwoDimRiemann);
    run_case(ProblemNo::TaylorGreen);
    return 0;
}
"""


def main():
    if not CXX.exists():
        raise SystemExit(f"compiler not found: {CXX}")
    temporary_root = ROOT / ".tmp"
    temporary_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="initial-lag-geometry-", dir=temporary_root
    ) as directory:
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
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source), str(ROOT / "src/alg.cpp"),
                "-o", str(executable),
            ],
            cwd=ROOT, env=environment, capture_output=True, text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return 1
        return subprocess.run([str(executable)], cwd=ROOT, env=environment).returncode


if __name__ == "__main__":
    raise SystemExit(main())
