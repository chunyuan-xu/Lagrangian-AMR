"""L6c micro-gate: regular force accessor shadow audit."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l6c-force-accessor.json"

SOURCE = r"""
#include <cassert>
#include <cstdint>
#include "defines.h"
#include "nodal/face_geometry_mirror_runtime.h"
#include "nodal/force_accessor_runtime.h"

using Nodal::Corner;
using Nodal::Vec2Storage;

static void set_half(quad_data_t &cell, int corner, int side,
	double nx, double ny, double length, double rcp)
{
	CHalf_edge_data &h = cell.m_cndata[corner].hdata[side];
	h.Ncp.x = nx;
	h.Ncp.y = ny;
	h.Lcp = length;
	h.Rcp = rcp;
}

static void set_unit_square_geometry(quad_data_t &cell)
{
	set_half(cell, 0, CHalf_edge_data::cside::plus, 1.0, 0.0, 0.5, 1.0);
	set_half(cell, 1, CHalf_edge_data::cside::minus, 1.0, 0.0, 0.5, 1.0);
	set_half(cell, 2, CHalf_edge_data::cside::plus, -1.0, 0.0, 0.5, 1.0);
	set_half(cell, 3, CHalf_edge_data::cside::minus, -1.0, 0.0, 0.5, 1.0);
	set_half(cell, 0, CHalf_edge_data::cside::minus, 0.0, -1.0, 0.5, 1.0);
	set_half(cell, 3, CHalf_edge_data::cside::plus, 0.0, -1.0, 0.5, 1.0);
	set_half(cell, 1, CHalf_edge_data::cside::plus, 0.0, 1.0, 0.5, 1.0);
	set_half(cell, 2, CHalf_edge_data::cside::minus, 0.0, 1.0, 0.5, 1.0);
}

int main()
{
	quad_data_t cell = {};
	set_unit_square_geometry(cell);
	assert(!Nodal::mirror_legacy_regular_geometry_to_faces(cell).failed);

	for (int c = 0; c < 4; ++c) {
		const Corner corner = static_cast<Corner>(c);
		const Vec2Storage legacy = Nodal::legacy_corner_geometry_force(cell, corner);
		const Vec2Storage current = Nodal::corner_geometry_force(cell.nodal.faces, corner);
		assert(Nodal::compare_vec(current, legacy));
	}

	cell.nodal.faces[static_cast<int>(Nodal::Face::Up)].segments[0].normal.y = 0.5;
	const Vec2Storage broken = Nodal::corner_geometry_force(cell.nodal.faces, Corner::LU);
	assert(!Nodal::compare_vec(broken,
		Nodal::legacy_corner_geometry_force(cell, Corner::LU)));
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l6c-force-accessor.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "all_corners_match": True,
        "corrupt_mismatch": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l6c-force-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-L6C {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
