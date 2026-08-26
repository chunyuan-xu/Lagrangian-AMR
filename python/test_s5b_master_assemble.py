"""S5b micro-gate: shadow master assembly."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s5b-master-assemble.json"

SOURCE = r"""
#include <cassert>
#include "nodal/master_assemble.h"

using Nodal::CellMasterContribution;
using Nodal::CondensedMasterContribution;
using Nodal::ShadowMaster;

int main()
{
	CellMasterContribution local = {};
	local.M[0][0] = 1.0;
	local.M[0][3] = 1.0;
	local.b[0] = 0.5;
	local.b[1] = -0.5;

	CondensedMasterContribution cond = {};
	cond.M[0][0] = 0.5;
	cond.M[0][3] = 0.5;
	cond.b[0] = 0.25;
	cond.b[1] = -0.25;

	ShadowMaster out = Nodal::assemble_shadow_master(local, 0, cond);
	assert(out.M.m[0][0] == 1.5);
	assert(out.M.m[1][1] == 1.5);
	assert(out.b[0] == 0.75);
	assert(out.b[1] == -0.75);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s5b-master-assemble.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "sum_direct_and_condensed": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s5b-assemble-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-SOLVE {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
