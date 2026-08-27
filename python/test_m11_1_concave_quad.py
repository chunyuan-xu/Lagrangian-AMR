"""M11.1 micro-gate: GeometryAlg::is_concave_quad predicate fixtures."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m11-1-concave-quad.json"

SOURCE = r"""
#include <cassert>
#include "alg.h"

int main()
{
    // Convex unit square: not concave.
    CDoubleVector square[4] = {
        CDoubleVector(0.0, 0.0),
        CDoubleVector(1.0, 0.0),
        CDoubleVector(1.0, 1.0),
        CDoubleVector(0.0, 1.0)
    };
    assert(GeometryAlg::is_concave_quad(square) == -1);

    // Concave quad: the D-A-B turn is negative.
    CDoubleVector concave[4] = {
        CDoubleVector(0.0, 0.0),
        CDoubleVector(2.0, 0.0),
        CDoubleVector(0.5, 0.5),
        CDoubleVector(0.0, 2.0)
    };
    assert(GeometryAlg::is_concave_quad(concave) >= 0);

    // Collinear degenerate: treated as not concave by the >= branch.
    CDoubleVector collinear[4] = {
        CDoubleVector(0.0, 0.0),
        CDoubleVector(1.0, 0.0),
        CDoubleVector(2.0, 0.0),
        CDoubleVector(3.0, 0.0)
    };
    assert(GeometryAlg::is_concave_quad(collinear) == -1);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m11-1-concave-quad.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "fixtures": ["convex", "concave", "collinear-degenerate"],
        "return_path_defined": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m11-1-", dir=ROOT / ".tmp") as directory:
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
                str(source), str(ROOT / 'src/alg.cpp'), "-o", str(executable),
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
    print(f"MG-M11-1 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
