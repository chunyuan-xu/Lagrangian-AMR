"""M12.1 micro-gate: Riemann targeted-trace diagnostics."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m12-1-riemann-target-trace.json"

SOURCE = r"""
#include <cassert>
#include <cstring>
#include "diagnostics/riemann_target_trace.h"

int main()
{
    char buf[256];
    int n = Diagnostics::riemann_trace_filename(buf, sizeof(buf), 4);
    assert(n >= 0);
    assert(n < (int)sizeof(buf));
    assert(std::strcmp(buf, "riemann_dbg_4.txt") == 0);

    // Disabled mode must return before dereferencing p4est.
    GhostSession session;
    Diagnostics::dump_riemann_target_if_enabled(NULL, session);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m12-1-riemann-target-trace.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "bounded_filename": False,
        "disabled_returns_early": False,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m12-1-", dir=ROOT / ".tmp") as directory:
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
        env.pop("LAGRANGIAN_TRACE_TARGET", None)
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
            if run.returncode == 0:
                summary["bounded_filename"] = True
                summary["disabled_returns_early"] = True
        else:
            summary["status"] = "FAIL"
            summary["compiler_output"] = (result.stdout + result.stderr)[-6000:]

    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-M12-1 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
