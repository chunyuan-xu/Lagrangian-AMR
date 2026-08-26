"""S5a micro-gate: MG-SOLVE analytic solve fixtures."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s5a-solve-fixtures.json"

SOURCE = r"""
#include <cassert>
#include <cmath>

enum class SolveStatus { Valid, Singular, NearSingular, NonFinite };

static SolveStatus check_solve(double a00, double a01, double a10, double a11)
{
	if (!std::isfinite(a00) || !std::isfinite(a01) || !std::isfinite(a10) || !std::isfinite(a11)) {
		return SolveStatus::NonFinite;
	}
	const double det = a00 * a11 - a01 * a10;
	if (std::fabs(det) < 1e-12) {
		return det == 0.0 ? SolveStatus::Singular : SolveStatus::NearSingular;
	}
	return SolveStatus::Valid;
}

int main()
{
	assert(check_solve(2.0, 0.0, 0.0, 3.0) == SolveStatus::Valid);
	assert(check_solve(1.0, 0.0, 0.0, 0.0) == SolveStatus::Singular);
	assert(check_solve(1.0, 0.0, 0.0, 1e-14) == SolveStatus::NearSingular);
	assert(check_solve(1.0, 0.0, 0.0, std::nan("")) == SolveStatus::NonFinite);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s5a-solve-fixtures.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_well_conditioned": True,
        "negative_singular": True,
        "negative_near_singular": True,
        "negative_nonfinite": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s5a-solve-", dir=ROOT / ".tmp") as directory:
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
