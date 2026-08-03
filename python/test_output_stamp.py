"""Compile and run the M1.5a output stamp unit test."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path("C:/msys64/ucrt64/bin/g++.exe")
SOURCE = r'''
#include <cassert>
#include "io/output_stamp.h"

int main()
{
    const auto first = IOAlgorithm::make_pre_step_stamp(1, 0.0);
    assert(first.file_step == 1);
    assert(first.state_step == 0);
    assert(first.time == 0.0);
    assert(first.phase == IOAlgorithm::OutputPhase::PreStep);
    assert(IOAlgorithm::phase_code(first.phase) == 0);

    const auto later = IOAlgorithm::make_pre_step_stamp(54, 0.0125);
    assert(later.file_step == 54);
    assert(later.state_step == 53);
    assert(later.time == 0.0125);

    const auto initial = IOAlgorithm::make_pre_step_stamp(0, 0.0);
    assert(initial.file_step == 0);
    assert(initial.state_step == 0);
    return 0;
}
'''


def main():
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_output_stamp_") as directory:
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
    print("PASS: pre-step output stamp maps file, state, time, and phase explicitly")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
