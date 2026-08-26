"""S5c micro-gate: local 2x2 shadow solve."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s5c-solve2x2.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/solve2x2.h"

using Nodal::ShadowMaster;

int main()
{
	ShadowMaster good = {};
	good.M = {{{2.0, 0.0}, {0.0, 3.0}}};
	good.b[0] = 4.0;
	good.b[1] = 6.0;
	Nodal::Solve2x2Result r = Nodal::solve_shadow_master(good);
	assert(r.ok);
	assert(std::fabs(r.velocity.x - 2.0) < 1e-12);
	assert(std::fabs(r.velocity.y - 2.0) < 1e-12);

	ShadowMaster singular = {};
	singular.M = {{{1.0, 0.0}, {0.0, 0.0}}};
	singular.b[0] = 1.0;
	singular.b[1] = 0.0;
	assert(!Nodal::solve_shadow_master(singular).ok);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s5c-solve2x2.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_solve": True,
        "negative_singular": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s5c-solve-", dir=ROOT / ".tmp") as directory:
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
