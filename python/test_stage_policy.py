"""Compile and run the M1.4 single-stage policy unit test."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path("C:/msys64/ucrt64/bin/g++.exe")
SOURCE = r'''
#include <cassert>
#include "physics/stage_policy.h"

int main()
{
    assert(StagePolicy::stage_count() == 1);
    assert(StagePolicy::timestep_scale(0) == 1.0);
    assert(StagePolicy::timestep_scale(-1) == 0.0);
    assert(StagePolicy::timestep_scale(1) == 0.0);
    assert(StagePolicy::accepts_after_stage(0));
    assert(!StagePolicy::accepts_after_stage(-1));
    assert(!StagePolicy::accepts_after_stage(1));
    return 0;
}
'''


def main():
    if not COMPILER.exists():
        raise FileNotFoundError(COMPILER)
    with tempfile.TemporaryDirectory(prefix="lagrangian_stage_policy_") as directory:
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
    print("PASS: single-stage policy has explicit timestep and acceptance semantics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
