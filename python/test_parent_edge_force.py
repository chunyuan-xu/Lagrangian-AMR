"""Focused poison/parity gate for the parent-edge force kernel."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-parent-edge-force-summary.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include "hydro/parent_edge_force.h"

CDoubleVector legacy_active_formula(
    const ParentBounInfo &info,
    const CDoubleVector &velocity,
    double pressure,
    const CDoubleMatrix &matrix)
{
    const CDoubleVector delta = info.Hanging_velocity - velocity;
    const CDoubleVector weighted =
        info.Lcp[0] * info.Ncp[0] + info.Lcp[1] * info.Ncp[1];
    const CDoubleVector matrix_delta(
        matrix.xx * delta.x + matrix.xy * delta.y,
        matrix.yx * delta.x + matrix.yy * delta.y);
    return weighted * pressure - matrix_delta;
}

int main()
{
    const double poison = std::numeric_limits<double>::quiet_NaN();
    ParentBounInfo inactive;
    inactive.IsParentChildBoun = false;
    inactive.Hanging_velocity = CDoubleVector(poison, poison);
    inactive.Ncp[0] = CDoubleVector(poison, poison);
    inactive.Ncp[1] = CDoubleVector(poison, poison);
    inactive.Lcp[0] = poison;
    inactive.Lcp[1] = poison;
    const CDoubleVector inactive_force = ParentEdgeForce::evaluate(
        inactive, CDoubleVector(poison, poison), poison,
        CDoubleMatrix(poison, poison, poison, poison));
    assert(std::isfinite(inactive_force.x));
    assert(std::isfinite(inactive_force.y));
    assert(inactive_force.x == 0.0);
    assert(inactive_force.y == 0.0);

    ParentBounInfo active;
    active.IsParentChildBoun = true;
    active.Hanging_velocity = CDoubleVector(3.0, -2.0);
    active.Ncp[0] = CDoubleVector(1.0, 0.5);
    active.Ncp[1] = CDoubleVector(-0.25, 2.0);
    active.Lcp[0] = 0.75;
    active.Lcp[1] = 1.25;
    const CDoubleVector velocity(-1.0, 4.0);
    const CDoubleMatrix matrix(2.0, -0.5, 1.5, 3.0);
    const CDoubleVector expected =
        legacy_active_formula(active, velocity, 7.0, matrix);
    const CDoubleVector actual =
        ParentEdgeForce::evaluate(active, velocity, 7.0, matrix);
    assert(std::memcmp(&actual.x, &expected.x, sizeof(double)) == 0);
    assert(std::memcmp(&actual.y, &expected.y, sizeof(double)) == 0);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.parent-edge-force.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 6,
        "fixture_count": 2,
        "inactive_poison_fixture": True,
        "active_bitwise_parity_fixture": True,
        "production_integration": False,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="parent-force-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", environment.get("PATH", "")]
        )
        result = subprocess.run(
            [
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
                "-IC:/msys64/ucrt64/include",
                str(source), "-o", str(executable),
            ],
            cwd=ROOT, env=environment, capture_output=True, text=True,
        )
        summary["compiler_exit_code"] = result.returncode
        if result.returncode == 0:
            run = subprocess.run([str(executable)], cwd=ROOT, env=environment)
            summary["executable_exit_code"] = run.returncode
            summary["status"] = "PASS" if run.returncode == 0 else "FAIL"
        else:
            summary["status"] = "FAIL"
            summary["compiler_output"] = (result.stdout + result.stderr)[-6000:]

    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-PARENT-EDGE-FORCE {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
