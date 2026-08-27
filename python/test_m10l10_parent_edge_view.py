"""M10L.10 micro-gate: ParentEdgeView address parity and const-write rejection."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m10l10-parent-edge-view.json"

SOURCE = r"""
#include <cassert>
#include <type_traits>
#include "defines.h"
#include "amr/parent_edge_view.h"

using AMRCallbacks::ParentEdgeView;

static_assert(std::is_same<decltype(std::declval<ParentEdgeView&>().at(0)),
    ParentBounInfo&>::value, "mutable view must return ParentBounInfo&");
static_assert(std::is_same<decltype(std::declval<const ParentEdgeView&>().at(0)),
    const ParentBounInfo&>::value, "const view must return const ParentBounInfo&");
static_assert(!std::is_assignable<const ParentBounInfo&, ParentBounInfo>::value,
    "const ParentBounInfo must reject assignment");

int main()
{
    quad_data_t cell = {};
    ParentEdgeView view(cell);
    for (int e = 0; e < view.size(); ++e) {
        assert(&view.at(e) == &cell.m_pc_edge_data[e]);
    }

    const quad_data_t &ccell = cell;
    const ParentEdgeView cview(ccell);
    const ParentBounInfo &ref = cview.at(0);
    assert(&ref == &ccell.m_pc_edge_data[0]);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m10l10-parent-edge-view.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "domain": "parent-edge",
        "consumer": "HydroPhases::quadrant_update_momentum_callback",
        "address_parity": True,
        "const_write_rejected": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m10l10-", dir=ROOT / ".tmp") as directory:
        build = Path(directory)
        source = build / "test.cpp"
        executable = build / "test.exe"
        source.write_text(SOURCE, encoding="ascii")
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join(
            ["C:/msys64/usr/bin", "C:/msys64/ucrt64/bin", env.get("PATH", "")]
        )
        env["TEMP"] = str(build)
        env["TMP"] = str(build)
        env["TMPDIR"] = str(build)
        result = subprocess.run(
            [
                str(CXX), "-O2", "-g", "-Wall", "-Wextra", "-std=c++14",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                "-IC:/Program Files (x86)/Microsoft SDKs/MPI/Include",
                "-IC:/msys64/ucrt64/include",
                str(source), "-o", str(executable),
            ],
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
    print(f"MG-M10L10 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
