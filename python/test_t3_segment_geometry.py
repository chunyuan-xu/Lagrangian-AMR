"""T3b micro-gate: MG-GEOM-P and pure MG-GEOM-C for segment geometry."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t3b-segment-geometry.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "nodal/segment_geometry.h"

int main()
{
    double start[2] = {0.0, 0.0}, end[2] = {1.0, 0.0};
    Nodal::EdgeSegmentGeometry seg;
    Nodal::SegmentGeometryError err =
        Nodal::build_regular_segment(start, end, seg, -1);
    assert(!err.failed);
    assert(std::fabs(seg.normal.x - 0.0) < 1e-12);
    assert(std::fabs(seg.normal.y - (-1.0)) < 1e-12);
    assert(std::fabs(seg.length - 1.0) < 1e-12);
    assert(std::fabs(seg.endpoint_weights[0] - 0.5) < 1e-12);
    assert(std::fabs(seg.endpoint_weights[1] - 0.5) < 1e-12);
    assert(!Nodal::validate_segment(seg).failed);

    double up[2] = {0.0, 1.0};
    err = Nodal::build_regular_segment(start, up, seg, -1);
    assert(!err.failed);
    assert(std::fabs(seg.normal.x - 1.0) < 1e-12);
    assert(std::fabs(seg.normal.y - 0.0) < 1e-12);

    double mid[2] = {0.5, 0.0};
    Nodal::EdgeSegmentGeometry pair[2];
    err = Nodal::build_split_segment_pair(start, mid, end, pair, -1);
    assert(!err.failed);
    assert(std::fabs(pair[0].length - 0.5) < 1e-12);
    assert(std::fabs(pair[1].length - 0.5) < 1e-12);
    assert(!Nodal::validate_segment(pair[0]).failed);
    assert(!Nodal::validate_segment(pair[1]).failed);
    assert(!Nodal::check_split_closure(start, end, pair).failed);
    assert(std::fabs(Nodal::total_segment_length(pair) - 1.0) < 1e-12);

    // Pure MG-GEOM-C: endpoint metric weights remain per-segment geometry and
    // sum to one; no solver activation is performed here.
    for (int i = 0; i < 2; ++i) {
        assert(std::fabs(pair[i].endpoint_weights[0] +
                         pair[i].endpoint_weights[1] - 1.0) < 1e-12);
    }

    double bad[2] = {0.0, 0.0};
    err = Nodal::build_regular_segment(start, bad, seg, -1);
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
        "schema": "lagrangian-amr.micro-gate.t3b-segment-geometry.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 14,
        "mg_geom_p": True,
        "mg_geom_c_pure": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="t3b-seg-", dir=ROOT / ".tmp") as directory:
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
    args.summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8")
    print(f"MG-T3B {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())