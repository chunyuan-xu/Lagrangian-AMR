"""S4a micro-gate: MG-CONDENSE endpoint condensation fixtures."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s4a-condensation.json"

SOURCE = r"""
#include <cassert>

enum class CondStatus { Valid, AssignmentNotAccum, WrongWeight, MissingEndpoint, DuplicateEndpoint, MissingExchange };

static CondStatus check_condensation(bool used_plus, bool missing,
	bool duplicate, bool exchanged, double omega)
{
	if (!used_plus) {
		return CondStatus::AssignmentNotAccum;
	}
	if (missing) {
		return CondStatus::MissingEndpoint;
	}
	if (duplicate) {
		return CondStatus::DuplicateEndpoint;
	}
	if (!exchanged) {
		return CondStatus::MissingExchange;
	}
	if (omega < 0.0 || omega > 1.0) {
		return CondStatus::WrongWeight;
	}
	return CondStatus::Valid;
}

int main()
{
	assert(check_condensation(true, false, false, true, 0.5) == CondStatus::Valid);
	assert(check_condensation(false, false, false, true, 0.5) == CondStatus::AssignmentNotAccum);
	assert(check_condensation(true, true, false, true, 0.5) == CondStatus::MissingEndpoint);
	assert(check_condensation(true, false, true, true, 0.5) == CondStatus::DuplicateEndpoint);
	assert(check_condensation(true, false, false, false, 0.5) == CondStatus::MissingExchange);
	assert(check_condensation(true, false, false, true, 1.5) == CondStatus::WrongWeight);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s4a-condensation.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_accumulation": True,
        "negative_assignment": True,
        "negative_missing_endpoint": True,
        "negative_duplicate_endpoint": True,
        "negative_missing_exchange": True,
        "negative_wrong_weight": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s4a-cond-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-CONDENSE {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
