"""S6a micro-gate: hanging velocity recovery."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s6a-hanging-recovery.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/hanging_recovery.h"

using Nodal::Vec2Storage;

int main()
{
	Vec2Storage a = {1.0, 2.0};
	Vec2Storage b = {3.0, 4.0};
	Vec2Storage h = Nodal::recover_hanging_velocity(a, b);
	assert(std::fabs(h.x - 2.0) < 1e-12);
	assert(std::fabs(h.y - 3.0) < 1e-12);

	Vec2Storage w = Nodal::recover_hanging_velocity(a, b, 0.25, 0.75);
	assert(std::fabs(w.x - 2.5) < 1e-12);
	assert(std::fabs(w.y - 3.5) < 1e-12);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s6a-hanging-recovery.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "equal_weights": True,
        "weighted_weights": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s6a-recover-", dir=ROOT / ".tmp") as directory:
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
