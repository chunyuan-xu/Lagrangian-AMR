"""S6d micro-gate: conjugate branch power and D_ch."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s6d-power-dissipation.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/local_algebra.h"

using Nodal::HalfEdgeInput;
using Nodal::LocalCornerInput;
using Nodal::LocalCornerOutput;

int main()
{
	LocalCornerInput in = {};
	in.z = 1.0;
	in.pressure = 1.0;
	in.u_c[0] = 0.0; in.u_c[1] = 0.0;
	in.u_k[0] = 1.0; in.u_k[1] = 0.0;
	in.e[0] = HalfEdgeInput{0.5, {1.0, 0.0}};
	in.e[1] = HalfEdgeInput{0.5, {0.0, -1.0}};
	LocalCornerOutput out = Nodal::build_local_corner(in);
	assert(out.dissipation >= -1e-12);
	const double branch_power = out.branch_force[0][0] * in.u_k[0] +
		out.branch_force[0][1] * in.u_k[1] +
		out.branch_force[1][0] * in.u_k[0] +
		out.branch_force[1][1] * in.u_k[1];
	assert(std::isfinite(branch_power));
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s6d-power-dissipation.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "D_ch_nonnegative": True,
        "branch_power_finite": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s6d-power-", dir=ROOT / ".tmp") as directory:
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
