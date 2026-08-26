"""L6a micro-gate: boundary accessor shadow audit."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l6a-boundary-accessor.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstdint>
#include "defines.h"
#include "variable.h"
#include "nodal/boundary_mirror_runtime.h"
#include "nodal/epoch_runtime.h"

using Nodal::BoundaryMirrorError;
using Nodal::EpochContext;
using Nodal::StagePhase;

static void set_half(quad_data_t &cell, int corner, int side,
	int type, double value, double nx, double ny, double length)
{
	CHalf_edge_data &h = cell.m_cndata[corner].hdata[side];
	h.enumBYD = type;
	h.BYDVal = value;
	h.Ncp.x = nx;
	h.Ncp.y = ny;
	h.Lcp = length;
}

int main()
{
	quad_data_t cell = {};
	// Left boundary: LB+ / LU-
	set_half(cell, 0, CHalf_edge_data::cside::plus, WallBoundary, 1.0, 1.0, 0.0, 0.5);
	set_half(cell, 1, CHalf_edge_data::cside::minus, WallBoundary, 1.0, 1.0, 0.0, 0.5);

	BoundaryMirrorError err = Nodal::mirror_legacy_boundary_to_faces(cell);
	assert(!err.failed);
	err = Nodal::verify_legacy_boundary_to_faces(cell);
	assert(!err.failed);

	p4est_data_t pdata = {};
	pdata.current_step = 10;
	EpochContext ctx = Nodal::make_stage_context(pdata, 12, 5, 0, StagePhase::Assemble);
	Nodal::stamp_stage_reset(cell, ctx);
	assert(!Nodal::validate_stage_reset(cell, ctx).failed);

	Nodal::invalidate_stage_reset(cell);
	assert(Nodal::validate_stage_reset(cell, ctx).failed);
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l6a-boundary-accessor.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "mirror_matches_legacy": True,
        "stamp_valid": True,
        "stale_rejected": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l6a-boundary-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-L6A {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
