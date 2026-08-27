"""M14.9 micro-gate: corner-work update kernel."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m14-9-work-kernel.json"

SOURCE = r"""
#include <cassert>
#include <cmath>
#include "hydro/work_kernel.h"

int main()
{
    CVariable vara = {};
    ParentBounInfo pc_info[CNDIM] = {};

    vara.cell_vector(idCentroidVelo_half) = CDoubleVector(1.0, 0.0);
    vara.cell_vector(idCentroidVelo_lag) = CDoubleVector(1.0, 0.0);
    for (int c = 0; c < CNDIM; ++c)
    {
        vara.corner_vector(idcnFcp, c) = CDoubleVector(1.0, 0.0);
        vara.corner_vector(idcnFluxRelaxed, c) = CDoubleVector(0.0, 0.0);
        vara.corner_vector(idcnVelocity_lag, c) = CDoubleVector(1.0, 0.0);
        vara.corner_vector(ideFcp, c) = CDoubleVector(0.0, 0.0);
        pc_info[c].IsParentChildBoun = false;
    }
    HydroCallbacks::update_work(
        vara, pc_info,
        static_cast<int>(p4est_data_t::MyCoordType::plane));
    assert(std::fabs(vara.cell(idKineticVariation) - 4.0) < 1e-12);
    assert(std::fabs(vara.cell(idTotalWork) - 4.0) < 1e-12);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m14-9-work-kernel.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "kernel_extracted": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m14-9-", dir=ROOT / ".tmp") as directory:
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
                str(source), str(ROOT / 'src/alg.cpp'), "-o", str(executable),
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
    print(f"MG-M14-9 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
