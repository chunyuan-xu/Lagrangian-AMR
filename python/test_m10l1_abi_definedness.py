"""M10L.1 micro-gate: ABI assertions plus a required-writer poison harness.

The selected required writer is Nodal::write_cell_local_master, which must keep
overwriting every slot of quad_data_t::nodal.master. If that writer or any of
its writes is removed, this fixture fails either at compile time (symbol
removal) or at runtime (poisoned values survive).
"""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m10l1-abi-definedness.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>
#include "defines.h"
#include "nodal/face_geometry_mirror_runtime.h"
#include "nodal/local_master_runtime.h"

template <typename T>
struct RawByteEligible : std::integral_constant<bool,
    std::is_standard_layout<T>::value &&
    std::is_trivially_copyable<T>::value &&
    std::is_trivially_destructible<T>::value> {};

static_assert(RawByteEligible<CDoubleVector>::value, "CDoubleVector raw ABI");
static_assert(RawByteEligible<CDoubleMatrix>::value, "CDoubleMatrix raw ABI");
static_assert(RawByteEligible<CPointBounInfo>::value, "CPointBounInfo raw ABI");
static_assert(RawByteEligible<CPoint_data_t>::value, "CPoint_data_t raw ABI");
static_assert(RawByteEligible<CHalf_edge_data>::value, "CHalf_edge_data raw ABI");
static_assert(RawByteEligible<CCorner_data>::value, "CCorner_data raw ABI");
static_assert(RawByteEligible<CEdge_data>::value, "CEdge_data raw ABI");
static_assert(RawByteEligible<ParentBounInfo>::value, "ParentBounInfo raw ABI");
static_assert(RawByteEligible<CVariable>::value, "CVariable raw ABI");
static_assert(RawByteEligible<quad_data_t>::value, "quad_data_t raw ABI");

static_assert(sizeof(CPointBounInfo) == 80, "CPointBounInfo size changed");
static_assert(sizeof(CPoint_data_t) == 384, "CPoint_data_t size changed");
static_assert(sizeof(CHalf_edge_data) == 104, "CHalf_edge_data size changed");
static_assert(sizeof(CCorner_data) == 208, "CCorner_data size changed");
static_assert(sizeof(CEdge_data) == 4, "CEdge_data size changed");
static_assert(sizeof(ParentBounInfo) == 104, "ParentBounInfo size changed");
static_assert(sizeof(CVariable) == 2992, "CVariable size changed");
static_assert(sizeof(quad_data_t) == 7232, "quad_data_t size changed");
static_assert(alignof(quad_data_t) == 8, "quad_data_t alignment changed");

static_assert(offsetof(quad_data_t, m_cndata) == 0, "m_cndata offset changed");
static_assert(offsetof(quad_data_t, m_edata) == 832, "m_edata offset changed");
static_assert(offsetof(quad_data_t, points) == 848, "points offset changed");
static_assert(offsetof(quad_data_t, init_node_coords) == 2384,
    "init_node_coords offset changed");
static_assert(offsetof(quad_data_t, m_pc_edge_data) == 2448,
    "m_pc_edge_data offset changed");
static_assert(offsetof(quad_data_t, m_vara) == 2864, "m_vara offset changed");
static_assert(offsetof(quad_data_t, face_neighbors) == 5856,
    "face_neighbors offset changed");
static_assert(offsetof(quad_data_t, face_num) == 5888, "face_num offset changed");
static_assert(offsetof(quad_data_t, nodal) == 5896, "nodal offset changed");

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
    const double poison = std::numeric_limits<double>::quiet_NaN();
    quad_data_t cell = {};
    set_unit_square_geometry(cell);
    assert(!Nodal::mirror_legacy_regular_geometry_to_faces(cell).failed);
    cell.m_vara.cell(idDensity_cur) = 1.0;
    cell.m_vara.cell(idSoundSpeed) = 1.0;
    cell.m_vara.cell(idPressure_cur) = 1.0;
    cell.m_vara.cell_vector(idCentroidVelo_cur) = CDoubleVector(0.0, 0.0);

    // The required writer must overwrite every master slot.
    for (int c = 0; c < CNDIM; ++c) {
        for (int j = 0; j < 4; ++j) {
            cell.nodal.master.M[c][j] = poison;
        }
        cell.nodal.master.b[2 * c] = poison;
        cell.nodal.master.b[2 * c + 1] = poison;
    }

    Nodal::write_cell_local_master(cell);

    for (int c = 0; c < CNDIM; ++c) {
        for (int j = 0; j < 4; ++j) {
            assert(std::isfinite(cell.nodal.master.M[c][j]));
        }
        assert(std::isfinite(cell.nodal.master.b[2 * c]));
        assert(std::isfinite(cell.nodal.master.b[2 * c + 1]));
    }
    assert(std::fabs(cell.nodal.master.M[0][0] - 0.5) < 1e-12);
    assert(std::fabs(cell.nodal.master.M[0][3] - 0.5) < 1e-12);
    assert(std::fabs(cell.nodal.master.b[0] - 0.5) < 1e-12);
    assert(std::fabs(cell.nodal.master.b[1] + 0.5) < 1e-12);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m10l1-abi-definedness.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 24,
        "poison_domain": "quad_data_t::nodal.master",
        "required_writer": "Nodal::write_cell_local_master",
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m10l1-abi-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", environment.get("PATH", "")]
        )
        environment["TEMP"] = str(build)
        environment["TMP"] = str(build)
        environment["TMPDIR"] = str(build)
        result = subprocess.run(
            [
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
                "-IC:/msys64/ucrt64/include",
                str(source), "-o", str(executable),
            ],
            cwd=ROOT, env=environment, capture_output=True, text=True,
        )
        summary["compiler_exit_code"] = result.returncode
        if result.returncode == 0:
            run = subprocess.run([str(executable)], cwd=ROOT, env=environment)
            summary["executable_exit_code"] = run.returncode
            summary["status"] = "PASS" if run.returncode == 0 else "FAIL"
        else:
            summary["status"] = "FAIL"
            summary["compiler_output"] = (result.stdout + result.stderr)[-6000:]

    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-M10L1 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
