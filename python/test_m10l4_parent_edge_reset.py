"""M10L.4 micro-gate: ParentBounInfo::FluxRelaxed reset contract."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m10l4-parent-edge-reset.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include "defines.h"
#include "amr/parent_edge_scratch.h"

int main()
{
    const double poison = std::numeric_limits<double>::quiet_NaN();

    // Inactive poison: an inactive slot must not leak poison after reset.
    ParentBounInfo inactive = {};
    inactive.IsParentChildBoun = false;
    inactive.FluxRelaxed = CDoubleVector(poison, poison);
    AMRCallbacks::reset_parent_edge_scratch(inactive);
    assert(std::isfinite(inactive.FluxRelaxed.x));
    assert(std::isfinite(inactive.FluxRelaxed.y));
    assert(inactive.FluxRelaxed.x == 0.0);
    assert(inactive.FluxRelaxed.y == 0.0);

    // Active parity: reset must clear FluxRelaxed regardless of active mask.
    ParentBounInfo active = {};
    active.IsParentChildBoun = true;
    active.FluxRelaxed = CDoubleVector(3.25, -1.5);
    AMRCallbacks::reset_parent_edge_scratch(active);
    assert(active.FluxRelaxed.x == 0.0);
    assert(active.FluxRelaxed.y == 0.0);

    // Byte scope: only the FluxRelaxed bytes change.
    ParentBounInfo actual = {};
    actual.IsParentChildBoun = true;
    actual.addDiss = true;
    actual.Ncp[0] = CDoubleVector(0.5, -2.0);
    actual.Ncp[1] = CDoubleVector(1.0, 0.25);
    actual.ParentPIStar = 7.0;
    actual.Lcp[0] = 0.75;
    actual.Lcp[1] = 1.25;
    actual.Zcp = 9.0;
    actual.Hanging_velocity = CDoubleVector(2.0, -3.0);
    actual.FluxRelaxed = CDoubleVector(poison, poison);

    ParentBounInfo expected = actual;
    expected.FluxRelaxed = CDoubleVector(0.0, 0.0);

    AMRCallbacks::reset_parent_edge_scratch(actual);
    assert(std::memcmp(&actual, &expected, sizeof(actual)) == 0);

    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m10l4-parent-edge-reset.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "slot": "ParentBounInfo::FluxRelaxed",
        "inactive_poison": True,
        "active_parity": True,
        "byte_scope": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m10l4-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-M10L4 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
