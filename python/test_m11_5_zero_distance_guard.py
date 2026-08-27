"""M11.5 micro-gate: pure zero-distance guard rejects invalid geometry."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m11-5-zero-distance-guard.json"

NORMAL = r"""
#include <cassert>
#include "alg.h"
int main()
{
    CDoubleVector a(0.0, 0.0);
    CDoubleVector b(1.0, 2.0);
    double d = GeometryAlg::guarded_point_distance(a, b, "normal");
    assert(d > 0.0);
    return 0;
}
"""

ZERO = r"""
#include "alg.h"
int main()
{
    CDoubleVector a(0.0, 0.0);
    CDoubleVector b(0.0, 0.0);
    GeometryAlg::guarded_point_distance(a, b, "zero");
    return 0;
}
"""


def compile_and_run(env, build, source_name, source_text, executable):
    source = build / source_name
    source.write_text(source_text, encoding="ascii")
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
    if result.returncode != 0:
        return {"compile": "FAIL", "compiler_output": (result.stdout + result.stderr)[-6000:]}
    run = subprocess.run([str(executable)], cwd=ROOT, env=env)
    return {"compile": "PASS", "exit": run.returncode}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m11-5-zero-distance-guard.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "normal_distance": False,
        "zero_distance_rejected": False,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m11-5-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")]
        )
        env["TEMP"] = str(build)
        env["TMP"] = str(build)
        env["TMPDIR"] = str(build)

        normal = compile_and_run(env, build, "normal.cpp", NORMAL, build / "normal.exe")
        zero = compile_and_run(env, build, "zero.cpp", ZERO, build / "zero.exe")
        summary["normal_compile"] = normal.get("compile")
        summary["normal_exit"] = normal.get("exit")
        summary["zero_compile"] = zero.get("compile")
        summary["zero_exit"] = zero.get("exit")
        if normal.get("compile") == "FAIL" or zero.get("compile") == "FAIL":
            summary["status"] = "FAIL"
            summary["compiler_output"] = normal.get("compiler_output") or zero.get("compiler_output")
        else:
            summary["normal_distance"] = normal["exit"] == 0
            summary["zero_distance_rejected"] = zero["exit"] != 0
            summary["status"] = "PASS" if (
                summary["normal_distance"] and summary["zero_distance_rejected"]
            ) else "FAIL"

    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-M11-5 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
