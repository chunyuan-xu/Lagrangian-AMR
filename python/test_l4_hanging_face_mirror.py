"""L4 micro-gate: MG-GEOM-C for hanging FaceData mirror helpers."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-l4-hanging-face-mirror.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "defines.h"
#include "nodal/face_geometry_mirror.h"
#include "nodal/face_geometry_mirror_runtime.h"

using Nodal::Face;
using Nodal::FaceData;
using Nodal::FaceGeometryMirrorError;
using Nodal::kFaceHangingFlag;
using Nodal::kFaceSegment1Present;

static void check_case(Face face, const std::uint8_t endpoints[2],
	const double normal[2], double length_a, double length_b,
	double rcp_a, double rcp_b)
{
	FaceData face_data = {};
	FaceGeometryMirrorError err = Nodal::build_hanging_face(face_data, face,
		endpoints, normal, length_a, length_b, rcp_a, rcp_b);
	assert(!err.failed);
	assert((face_data.flags & kFaceSegment1Present) != 0);
	assert((face_data.flags & kFaceHangingFlag) != 0);
	assert(std::fabs(face_data.segments[0].length - length_a) < 1e-12);
	assert(std::fabs(face_data.segments[1].length - length_b) < 1e-12);
	assert(std::fabs(face_data.segments[0].normal.x - normal[0]) < 1e-12);
	assert(std::fabs(face_data.segments[1].normal.y - normal[1]) < 1e-12);

	err = Nodal::verify_hanging_face(face_data, face, endpoints,
		normal, length_a, length_b, rcp_a, rcp_b);
	assert(!err.failed);

	// Fine-side order swap is only a different length assignment to the two
	// physical segments, not a different face.
	err = Nodal::verify_hanging_face(face_data, face, endpoints,
		normal, length_b, length_a, rcp_b, rcp_a);
	assert(err.failed);
}

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
	const double nx = 1.0, ny = 0.0;
	const double nx_right = -1.0;
	const double bottom_normal[2] = {0.0, -1.0};
	const double up_normal[2] = {0.0, 1.0};
	const double left_normal[2] = {nx, ny};
	const double right_normal[2] = {nx_right, ny};
	std::uint8_t left[2] = {0, 1};
	std::uint8_t right[2] = {3, 2};
	std::uint8_t bottom[2] = {0, 3};
	std::uint8_t up[2] = {1, 2};

	check_case(Face::Left, left, left_normal, 0.25, 0.35, 1.0, 1.0);
	check_case(Face::Right, right, right_normal, 0.35, 0.25, 1.0, 1.0);
	check_case(Face::Bottom, bottom, bottom_normal, 0.25, 0.35, 1.0, 1.0);
	check_case(Face::Up, up, up_normal, 0.35, 0.25, 1.0, 1.0);

	// Different metric weights should also round trip.
	check_case(Face::Left, left, left_normal, 0.25, 0.35, 0.75, 1.25);

	// Runtime mirror: a coarse leaf with a hanging left face must produce
	// two physical segments and verify against legacy.
	quad_data_t cell = {};
	set_unit_square_geometry(cell);
	cell.m_pc_edge_data[static_cast<int>(Face::Left)].IsParentChildBoun = true;
	cell.m_cndata[0].hdata[CHalf_edge_data::cside::plus].Lcp = 0.4;
	cell.m_cndata[1].hdata[CHalf_edge_data::cside::minus].Lcp = 0.6;

	FaceGeometryMirrorError err = Nodal::mirror_legacy_geometry_to_faces(cell);
	if (err.failed) { fprintf(stderr, "mirror failed: %s\n", err.reason ? err.reason : "?"); return 1; }
	assert((cell.nodal.faces[static_cast<int>(Face::Left)].flags & kFaceSegment1Present) != 0);
	assert((cell.nodal.faces[static_cast<int>(Face::Left)].flags & kFaceHangingFlag) != 0);
	assert(std::fabs(cell.nodal.faces[static_cast<int>(Face::Left)].segments[0].length - 0.4) < 1e-12);
	assert(std::fabs(cell.nodal.faces[static_cast<int>(Face::Left)].segments[1].length - 0.6) < 1e-12);
	err = Nodal::verify_legacy_geometry_to_faces(cell);
	if (err.failed) { fprintf(stderr, "verify failed: %s\n", err.reason ? err.reason : "?"); return 1; }

	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.l4-hanging-face-mirror.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "directions": 4,
        "fine_side_orders": True,
        "metric_weights": True,
        "negative_order_swap": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="l4-hanging-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-GEOM-C {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
