"""L5a micro-gate: MG-EPOCH stage-reset stamping."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l5a-stage-reset.json"

SOURCE = r"""
#include <cassert>
#include <cstdint>
#include "defines.h"
#include "nodal/epoch_runtime.h"

using Nodal::EpochContext;
using Nodal::StagePhase;

int main()
{
	p4est_data_t data = {};
	data.current_step = 3;
	EpochContext ctx = Nodal::make_stage_context(data, 5, 2, 0, StagePhase::Assemble);
	assert(ctx.generation == 5);
	assert(ctx.topology_version == 2);
	assert(ctx.step == 3);

	quad_data_t cell = {};
	assert(Nodal::validate_stage_reset(cell, ctx).failed);

	Nodal::stamp_stage_reset(cell, ctx);
	assert(!Nodal::validate_stage_reset(cell, ctx).failed);

	EpochContext wrong = ctx;
	wrong.step++;
	assert(Nodal::validate_stage_reset(cell, wrong).failed);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l5a-stage-reset.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "initial_invalid": True,
        "stage_stamp_valid": True,
        "wrong_epoch_rejected": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l5a-stage-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")])
        env["TEMP"] = str(build)
        env["TMP"] = str(build)
        env["TMPDIR"] = str(build)
        result = subprocess.run(
            [str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
             f"-I{ROOT / 'src'}",
             f"-I{ROOT / 'third_party/p4est/build/local/include'}",
             "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
             "-IC:/msys64/ucrt64/include",
             str(source), "-o", str(executable)],
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
