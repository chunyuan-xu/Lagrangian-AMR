"""S3a micro-gate: MG-MPI face-ledger fixtures for hanging owner uniqueness."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s3a-face-ledger.json"

SOURCE = r"""
#include <cassert>
#include <cstdint>

struct LedgerEntry {
	std::int32_t owner;
	bool is_ghost;
};

enum class LedgerStatus { Valid, MissingOwner, DuplicateOwner, GhostWrite };

static LedgerStatus check_ledger(const LedgerEntry *entries, int count)
{
	int owner_count = 0;
	bool ghost_write = false;
	for (int i = 0; i < count; ++i) {
		if (entries[i].owner >= 0) {
			++owner_count;
			if (entries[i].is_ghost) {
				ghost_write = true;
			}
		}
	}
	if (ghost_write) {
		return LedgerStatus::GhostWrite;
	}
	if (owner_count == 0) {
		return LedgerStatus::MissingOwner;
	}
	if (owner_count > 1) {
		return LedgerStatus::DuplicateOwner;
	}
	return LedgerStatus::Valid;
}

int main()
{
	LedgerEntry good[2] = {{0, false}, {-1, false}};
	assert(check_ledger(good, 2) == LedgerStatus::Valid);

	LedgerEntry missing[2] = {{-1, false}, {-1, false}};
	assert(check_ledger(missing, 2) == LedgerStatus::MissingOwner);

	LedgerEntry duplicate[2] = {{0, false}, {0, false}};
	assert(check_ledger(duplicate, 2) == LedgerStatus::DuplicateOwner);

	LedgerEntry ghost[2] = {{1, true}, {-1, false}};
	assert(check_ledger(ghost, 2) == LedgerStatus::GhostWrite);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s3a-face-ledger.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "positive_unique_owner": True,
        "negative_missing_owner": True,
        "negative_duplicate_owner": True,
        "negative_ghost_write": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s3a-ledger-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-MPI {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
