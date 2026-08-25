"""T2b1 micro-gate: pure topology mapping (MG-TOPO pure)."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t2b1-topology-mapping.json"

SOURCE = r"""
#include <cassert>
#include "nodal/topology_mapping.h"

int main()
{
    std::uint8_t endpoints[2];

    Nodal::MappingError err = Nodal::face_endpoints(Nodal::Face::Left, endpoints);
    assert(!err.failed); assert(endpoints[0] == 0); assert(endpoints[1] == 1);
    err = Nodal::face_endpoints(Nodal::Face::Right, endpoints);
    assert(!err.failed); assert(endpoints[0] == 3); assert(endpoints[1] == 2);
    err = Nodal::face_endpoints(Nodal::Face::Bottom, endpoints);
    assert(!err.failed); assert(endpoints[0] == 0); assert(endpoints[1] == 3);
    err = Nodal::face_endpoints(Nodal::Face::Up, endpoints);
    assert(!err.failed); assert(endpoints[0] == 1); assert(endpoints[1] == 2);

    const std::uint8_t expected_p4est[] = {0, 3, 1, 2};
    for (std::uint8_t i = 0; i < 4; ++i) {
        assert(Nodal::quad_corner_from_p4est(i) == expected_p4est[i]);
    }
    assert(Nodal::quad_corner_from_p4est(9) == 0xFF);

    std::uint8_t rev[2] = {0, 1};
    Nodal::reverse_endpoints(rev);
    assert(rev[0] == 1 && rev[1] == 0);

    int sibling = -1, child = -1;
    assert(!Nodal::fine_sibling_order(Nodal::Face::Bottom, 0, sibling, child).failed);
    assert(sibling == static_cast<int>(Nodal::Face::Bottom) && child == 3);
    assert(!Nodal::fine_sibling_order(Nodal::Face::Up, 1, sibling, child).failed);
    assert(child == 2);

    Nodal::LeafCellKey a{0, 5, 4, 4};
    Nodal::LeafCellKey b{0, 5, 4, 4};
    Nodal::LeafCellKey c{1, 5, 4, 4};
    assert(a == b); assert(!(a == c));

    Nodal::CanonicalNodeKey k1 = Nodal::canonical_node_key_from_point(0.5, 0.5);
    Nodal::CanonicalNodeKey k2 = Nodal::canonical_node_key_from_point(0.5, 0.5);
    Nodal::CanonicalNodeKey k3 = Nodal::canonical_node_key_from_point(0.5, 0.501);
    assert(k1 == k2); assert(!(k1 == k3));

    Nodal::CanonicalHangingFaceKey f1 =
        Nodal::canonical_hanging_face_key(k1, k3, 1);
    Nodal::CanonicalHangingFaceKey f2 =
        Nodal::canonical_hanging_face_key(k3, k1, 1);
    assert(f1 == f2);

    err = Nodal::face_endpoints(static_cast<Nodal::Face>(99), endpoints);
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
        "schema": "lagrangian-amr.micro-gate.t2b1-topology-mapping.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 18,
        "corner_orders": {
            "quad": ["LB", "LU", "RU", "RB"],
            "p4est": ["LB", "RB", "LU", "RU"],
        },
        "edge_orders": [
            ["left", "right", "bottom", "up"],
            ["left", "up", "right", "bottom"],
        ],
        "invalid_input_fail_fast": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="t2b1-map-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-T2B1 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())