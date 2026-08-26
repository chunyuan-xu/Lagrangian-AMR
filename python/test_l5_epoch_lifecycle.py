"""L5-0 micro-gate: MG-EPOCH/MG-AMR infrastructure for stale-read rejection."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l5-epoch-lifecycle.json"

SOURCE = r"""
#include <cassert>
#include <cstdint>
#include "nodal/epoch_lifecycle.h"

using Nodal::EpochContext;
using Nodal::EpochError;
using Nodal::StagePhase;
using Nodal::StageStamp;

static EpochContext ctx()
{
	EpochContext c = {7, 3, 42, 1, StagePhase::Assemble};
	return c;
}

int main()
{
	EpochContext c = ctx();
	StageStamp stamp = Nodal::make_stamp(c);
	assert(stamp.validity == static_cast<std::uint8_t>(Nodal::ValidityFlag::Valid));
	assert(!Nodal::read_guard(stamp, c).failed);

	Nodal::invalidate_stamp(stamp);
	EpochError err = Nodal::read_guard(stamp, c);
	assert(err.failed);

	stamp = Nodal::make_stamp(c);
	EpochContext wrong = c;
	wrong.generation++;
	assert(Nodal::read_guard(stamp, wrong).failed);

	stamp = Nodal::make_stamp(c);
	wrong = c;
	wrong.topology_version++;
	assert(Nodal::read_guard(stamp, wrong).failed);

	stamp = Nodal::make_stamp(c);
	wrong = c;
	wrong.step++;
	assert(Nodal::read_guard(stamp, wrong).failed);

	stamp = Nodal::make_stamp(c);
	wrong = c;
	wrong.sub_stage++;
	assert(Nodal::read_guard(stamp, wrong).failed);

	stamp = Nodal::make_stamp(c);
	wrong = c;
	wrong.phase = StagePhase::Solve;
	assert(Nodal::read_guard(stamp, wrong).failed);

	// stamp_current should refresh to the new epoch.
	stamp = Nodal::make_stamp(c);
	Nodal::stamp_current(stamp, wrong);
	assert(!Nodal::read_guard(stamp, wrong).failed);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l5-epoch-lifecycle.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "valid_stamp": True,
        "invalid_stale_rejected": True,
        "mismatch_cases": 5,
        "refresh_case": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l5-epoch-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-EPOCH {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
