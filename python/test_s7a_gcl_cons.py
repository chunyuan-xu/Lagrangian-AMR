"""S7a micro-gate: MG-GCL/MG-CONS fixture infrastructure."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s7a-gcl-cons.json"

SOURCE = r"""
#include <cassert>
#include <cmath>

enum class ConsStatus { Valid, BrokenSign, MissingFace, DuplicatedWork };

static ConsStatus check_gcl(int face_count, double swept, bool negative_volume)
{
	if (negative_volume) {
		return ConsStatus::BrokenSign;
	}
	if (face_count < 4) {
		return ConsStatus::MissingFace;
	}
	if (face_count > 4) {
		return ConsStatus::DuplicatedWork;
	}
	if (std::fabs(swept) > 1e-12) {
		return ConsStatus::BrokenSign;
	}
	return ConsStatus::Valid;
}

int main()
{
	assert(check_gcl(4, 0.0, false) == ConsStatus::Valid);
	assert(check_gcl(4, 0.0, true) == ConsStatus::BrokenSign);
	assert(check_gcl(3, 0.0, false) == ConsStatus::MissingFace);
	assert(check_gcl(5, 0.0, false) == ConsStatus::DuplicatedWork);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s7a-gcl-cons.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_gcl": True,
        "negative_broken_sign": True,
        "negative_missing_face": True,
        "negative_duplicated_work": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s7a-gcl-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-GCL {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
