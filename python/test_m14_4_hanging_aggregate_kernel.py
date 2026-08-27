"""M14.4 micro-gate: hanging aggregate matrix/RHS kernel."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m14-4-hanging-aggregate-kernel.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "hydro/hanging_aggregate_kernel.h"

int main()
{
    CDoubleMatrix m0(1.0, 0.0, 0.0, 1.0);
    CDoubleMatrix m1(2.0, 0.0, 0.0, 2.0);
    CDoubleMatrix m2(3.0, 0.0, 0.0, 3.0);
    CDoubleVector r0(1.0, 2.0);
    CDoubleVector r1(3.0, 4.0);
    CDoubleVector r2(5.0, 6.0);
    HydroCallbacks::HangingAggregate out =
        HydroCallbacks::aggregate_hanging_matrix_rhs(m0, r0, m1, r1, m2, r2);
    assert(std::fabs(out.matrix.xx - 6.0) < 1e-12);
    assert(std::fabs(out.matrix.yy - 6.0) < 1e-12);
    assert(std::fabs(out.rhs.x - 9.0) < 1e-12);
    assert(std::fabs(out.rhs.y - 12.0) < 1e-12);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m14-4-hanging-aggregate-kernel.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "kernel_extracted": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m14-4-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")]
        )
        env["TEMP"] = str(build)
        env["TMP"] = str(build)
        env["TMPDIR"] = str(build)
        result = subprocess.run(
            [
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
                "-IC:/msys64/ucrt64/include",
                str(source), "-o", str(executable),
            ],
            cwd=ROOT, env=env, capture_output=True, text=True,
        )
        summary["compiler_exit_code"] = result.returncode
        if result.returncode == 0:
            run = subprocess.run([str(executable)], cwd=ROOT, env=env)
            summary["executable_exit_code"] = run.returncode
            summary["status"] = "PASS" if run.returncode == 0 else "FAIL"
        else:
            summary["status"] = "FAIL"
            summary["compiler_output"] = (result.stdout + result.stderr)[-6000:]

    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-M14-4 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
