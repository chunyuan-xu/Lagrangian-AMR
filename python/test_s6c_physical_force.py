"""S6c micro-gate: physical cell force recovery."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s6c-physical-force.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/local_algebra.h"

using Nodal::HalfEdgeInput;
using Nodal::LocalCornerInput;
using Nodal::LocalCornerOutput;

static double physical_x(const LocalCornerInput &in)
{
	return Nodal::build_local_corner(in).physical_force[0];
}

int main()
{
	LocalCornerInput a = {};
	a.z = 1.0; a.pressure = 1.0;
	a.u_c[0] = 0.0; a.u_c[1] = 0.0;
	a.u_k[0] = 0.0; a.u_k[1] = 0.0;
	a.e[0] = HalfEdgeInput{0.5, {1.0, 0.0}};
	a.e[1] = HalfEdgeInput{0.5, {0.0, -1.0}};
	LocalCornerInput b = a;
	b.e[0] = HalfEdgeInput{0.5, {-1.0, 0.0}};
	b.e[1] = HalfEdgeInput{0.5, {0.0, 1.0}};
	const double total = physical_x(a) + physical_x(b);
	assert(std::fabs(total) < 1e-12);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s6c-physical-force.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "cell_force_cancel": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s6c-force-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")])
        result = subprocess.run(
            [str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
             f"-I{ROOT / 'src'}", str(source), "-o", str(executable)],
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
    print(f"MG-FORCE {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
