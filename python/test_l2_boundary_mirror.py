"""L2b micro-gate: MG-BOUNDARY for legacy boundary mirror helpers."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l2-boundary-mirror.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "defines.h"
#include "variable.h"
#include "nodal/boundary_mirror_runtime.h"

using Nodal::BoundaryMirrorError;
using Nodal::BoundaryRecord;
using Nodal::Face;
using Nodal::CellNodalData;

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

static void set_regular_boundary(quad_data_t &cell)
{
	// Left: LB+ / LU-
	set_half(cell, 0, CHalf_edge_data::cside::plus, WallBoundary, 1.0, 1.0, 0.0, 0.5);
	set_half(cell, 1, CHalf_edge_data::cside::minus, WallBoundary, 1.0, 1.0, 0.0, 0.5);
	// Right: RU+ / RB-
	set_half(cell, 2, CHalf_edge_data::cside::plus, WallBoundary, 2.0, -1.0, 0.0, 0.5);
	set_half(cell, 3, CHalf_edge_data::cside::minus, WallBoundary, 2.0, -1.0, 0.0, 0.5);
	// Bottom: LB- / RB+
	set_half(cell, 0, CHalf_edge_data::cside::minus, WallBoundary, 3.0, 0.0, -1.0, 0.5);
	set_half(cell, 3, CHalf_edge_data::cside::plus, WallBoundary, 3.0, 0.0, -1.0, 0.5);
	// Up: LU+ / RU-
	set_half(cell, 1, CHalf_edge_data::cside::plus, WallBoundary, 4.0, 0.0, 1.0, 0.5);
	set_half(cell, 2, CHalf_edge_data::cside::minus, WallBoundary, 4.0, 0.0, 1.0, 0.5);
}

int main()
{
	// Pure build/verify round trip on a fully-boundary regular leaf.
	quad_data_t cell = {};
	set_regular_boundary(cell);
	BoundaryMirrorError err = Nodal::mirror_legacy_boundary_to_faces(cell);
	assert(!err.failed);

	assert(cell.nodal.boundaries[static_cast<int>(Face::Left)].type == WallBoundary);
	assert(cell.nodal.boundaries[static_cast<int>(Face::Left)].value == 1.0);
	assert(std::fabs(cell.nodal.boundaries[static_cast<int>(Face::Left)].normal.x - 1.0) < 1e-12);
	assert(std::fabs(cell.nodal.boundaries[static_cast<int>(Face::Left)].length - 1.0) < 1e-12);

	assert(cell.nodal.boundaries[static_cast<int>(Face::Right)].value == 2.0);
	assert(std::fabs(cell.nodal.boundaries[static_cast<int>(Face::Right)].normal.x + 1.0) < 1e-12);
	assert(cell.nodal.boundaries[static_cast<int>(Face::Bottom)].value == 3.0);
	assert(std::fabs(cell.nodal.boundaries[static_cast<int>(Face::Bottom)].normal.y + 1.0) < 1e-12);
	assert(cell.nodal.boundaries[static_cast<int>(Face::Up)].value == 4.0);
	assert(std::fabs(cell.nodal.boundaries[static_cast<int>(Face::Up)].normal.y - 1.0) < 1e-12);

	err = Nodal::verify_legacy_boundary_to_faces(cell);
	assert(!err.failed);

	// Inner leaf mirrors to type=0 and verifies cleanly.
	quad_data_t inner = {};
	err = Nodal::mirror_legacy_boundary_to_faces(inner);
	assert(!err.failed);
	for (int f = 0; f < 4; ++f) {
		assert(inner.nodal.boundaries[f].type == 0);
		assert(std::fabs(inner.nodal.boundaries[f].length) < 1e-12);
	}
	err = Nodal::verify_legacy_boundary_to_faces(inner);
	assert(!err.failed);

	// Mismatch must be caught: flip one endpoint's boundary type.
	quad_data_t bad = {};
	set_regular_boundary(bad);
	bad.m_cndata[1].hdata[CHalf_edge_data::cside::minus].enumBYD = FreeBoundary;
	err = Nodal::mirror_legacy_boundary_to_faces(bad);
	assert(err.failed);

	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l2-boundary-mirror.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "pure_helpers": True,
        "runtime_mirror": True,
        "negative_mismatch": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l2-boundary-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-BOUNDARY {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
