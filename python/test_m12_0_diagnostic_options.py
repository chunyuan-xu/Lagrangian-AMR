"""M12.0 micro-gate: immutable diagnostic startup options."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m12-0-diagnostic-options.json"

SOURCE = r"""
#include <cassert>
#include "diagnostics/diagnostic_options.h"
#include "core/trace.h"

int main()
{
    const Diagnostics::DiagnosticOptions &opts =
        Diagnostics::DiagnosticOptions::instance();
    // Child process has no diagnostic env flags set.
    assert(!opts.trace_target());
    assert(!opts.trace_refine());
    assert(!opts.verbose_amr());
    assert(!opts.checksum_trace());
    assert(!opts.refresh_idempotence());
    assert(!opts.state_invariant());
    assert(!opts.memory_high_water());
    assert(!target_trace_enabled());
    assert(!refine_trace_enabled());
    assert(trace_riemann_iter() == -1);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m12-0-diagnostic-options.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "immutable_options": True,
        "disabled_defaults": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2

    with tempfile.TemporaryDirectory(prefix="m12-0-", dir=ROOT / ".tmp") as directory:
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
        for key in (
            "LAGRANGIAN_TRACE_TARGET", "LAGRANGIAN_TRACE_REFINE",
            "LAGRANGIAN_VERBOSE_AMR", "LAGRANGIAN_TRACE_CHECKSUM",
            "LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE",
            "LAGRANGIAN_CHECK_STATE_INVARIANTS", "LAGRANGIAN_MEMORY_HIGH_WATER",
        ):
            env.pop(key, None)
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
    print(f"MG-M12-0 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
