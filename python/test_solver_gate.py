"""Compile and run the M1.3 solver/coordinate gate unit test."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path("C:/msys64/ucrt64/bin/g++.exe")
SOURCE = r'''
#include <cassert>
#include "solver/solver_gate.h"

int main()
{
    using SolverGate::CoordinateType;
    using SolverGate::SolverType;

    assert(SolverGate::should_run_riemann(CoordinateType::Plane, SolverType::GridAligned));
    assert(SolverGate::should_run_riemann(CoordinateType::Plane, SolverType::Rotated));
    assert(!SolverGate::should_run_riemann(CoordinateType::Cylinder, SolverType::GridAligned));
    assert(!SolverGate::should_run_riemann(CoordinateType::Cylinder, SolverType::Rotated));

    assert(SolverGate::is_valid_coordinate_type(0));
    assert(SolverGate::is_valid_coordinate_type(1));
    assert(!SolverGate::is_valid_coordinate_type(-1));
    assert(!SolverGate::is_valid_coordinate_type(2));
    assert(SolverGate::is_valid_solver_type(0));
    assert(SolverGate::is_valid_solver_type(1));
    assert(!SolverGate::is_valid_solver_type(-1));
    assert(!SolverGate::is_valid_solver_type(2));

    assert(SolverGate::coordinate_type_from_legacy(0) == CoordinateType::Plane);
    assert(SolverGate::coordinate_type_from_legacy(1) == CoordinateType::Cylinder);
    assert(SolverGate::solver_type_from_legacy(0) == SolverType::GridAligned);
    assert(SolverGate::solver_type_from_legacy(1) == SolverType::Rotated);
    return 0;
}
'''


def main():
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_solver_gate_") as directory:
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
    print("PASS: solver gate covers all coordinate/solver combinations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
