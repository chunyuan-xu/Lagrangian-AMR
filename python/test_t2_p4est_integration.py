"""T2b2 micro-gate: real multi-tree p4est connectivity integration fixture."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t2b2-p4est-integration.json"

SOURCE = r"""
#include <p4est.h>
#include <cassert>
#include "nodal/topology_mapping.h"

int main()
{
    // Two trees joined along a face.  Tree 0 right face joins tree 1 left
    // face with orientation 0.
    p4est_connectivity_t *conn =
        p4est_connectivity_new_twotrees(1,
                                        0,
                                        0);
    assert(conn != NULL);
    assert(conn->num_trees == 2);

    double v0[3], v1[3];
    // Tree 0 right-bottom and tree 1 left-bottom are the same physical point.
    p4est_qcoord_to_vertex(conn, 0, P4EST_ROOT_LEN, 0, v0);
    p4est_qcoord_to_vertex(conn, 1, 0, 0, v1);

    Nodal::CanonicalNodeKey k0 =
        Nodal::canonical_node_key_from_point(v0[0], v0[1]);
    Nodal::CanonicalNodeKey k1 =
        Nodal::canonical_node_key_from_point(v1[0], v1[1]);
    assert(k0 == k1);

    // Tree 0 right-up and tree 1 left-up are also the same physical point.
    p4est_qcoord_to_vertex(conn, 0, P4EST_ROOT_LEN, P4EST_ROOT_LEN, v0);
    p4est_qcoord_to_vertex(conn, 1, 0, P4EST_ROOT_LEN, v1);
    Nodal::CanonicalNodeKey k2 =
        Nodal::canonical_node_key_from_point(v0[0], v0[1]);
    Nodal::CanonicalNodeKey k3 =
        Nodal::canonical_node_key_from_point(v1[0], v1[1]);
    assert(k2 == k3);

    // Different physical points must produce different keys.
    assert(!(k0 == k3));

    p4est_connectivity_destroy(conn);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.t2b2-p4est-integration.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 5,
        "multi_tree_connectivity": "twotrees",
        "cross_tree_same_node": True,
        "cross_tree_different_node": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="t2b2-p4est-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")])
        compile_cmd = [
            str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
            f"-I{ROOT / 'src'}",
            f"-I{ROOT / 'third_party/p4est/build/local/include'}",
            "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
            "-IC:/msys64/ucrt64/include",
            f"-L{ROOT / 'third_party/p4est/build/local/lib'}",
            "-LC:/msys64/ucrt64/lib",
            str(source), "-lp4est", "-lsc", "-lz", "-lmsmpi", "-lws2_32",
            "-o", str(executable),
        ]
        result = subprocess.run(compile_cmd, cwd=ROOT, env=env,
                                capture_output=True, text=True)
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
    print(f"MG-T2B2 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())