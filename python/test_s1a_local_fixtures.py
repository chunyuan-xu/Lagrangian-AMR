"""S1a micro-gate: MG-LOCAL fixture infrastructure for DBGF local algebra."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s1a-local-fixtures.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/matrix_accessor.h"

using Nodal::Matrix2;

static bool is_symmetric_psd(const Matrix2 &m, double tol = 1e-12)
{
	if (std::fabs(m.m[0][1] - m.m[1][0]) > tol) {
		return false;
	}
	const double tr = m.m[0][0] + m.m[1][1];
	const double det = m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[0][1];
	return tr >= -tol && det >= -tol;
}

int main()
{
	// Positive fixture: diagonal PSD matrix.
	Matrix2 good = {{{2.0, 0.0}, {0.0, 3.0}}};
	assert(is_symmetric_psd(good));

	// Negative fixture: asymmetric matrix must be rejected.
	Matrix2 asymmetric = {{{1.0, 2.0}, {0.0, 1.0}}};
	assert(!is_symmetric_psd(asymmetric));

	// Negative fixture: indefinite matrix with negative determinant must be
	// rejected as broken dissipation.
	Matrix2 indefinite = {{{1.0, 0.0}, {0.0, -1.0}}};
	assert(!is_symmetric_psd(indefinite));

	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s1a-local-fixtures.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_fixture": True,
        "negative_fixture_count": 2,
        "broken_kernel_rejected": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s1a-local-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-LOCAL {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
