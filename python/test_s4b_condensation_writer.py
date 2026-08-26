"""S4b micro-gate: MG-CONDENSE endpoint condensation writer."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-s4b-condensation-writer.json"

SOURCE = r"""
#include <cassert>
#include "nodal/condensation.h"

using Nodal::AggregatedHangingContribution;
using Nodal::CondensedMasterContribution;
using Nodal::HangingAggregateInput;

int main()
{
	HangingAggregateInput in = {};
	in.M_a = {{{1.0, 0.0}, {0.0, 1.0}}};
	in.M_b = {{{2.0, 0.0}, {0.0, 2.0}}};
	in.b_a[0] = 1.0; in.b_a[1] = 2.0;
	in.b_b[0] = 3.0; in.b_b[1] = 4.0;
	AggregatedHangingContribution agg;
	Nodal::aggregate_hanging_contribution(in, agg);

	CondensedMasterContribution out = {};
	assert(Nodal::apply_endpoint_condensation(out, agg, 0.5, 0));
	assert(Nodal::apply_endpoint_condensation(out, agg, 0.5, 1));
	assert(out.M[0][0] == 0.5);
	assert(out.M[0][3] == 0.5);
	assert(out.M[1][0] == 1.0);
	assert(out.M[1][3] == 1.0);
	assert(out.b[0] == 0.5);
	assert(out.b[1] == 1.0);
	assert(out.b[2] == 1.5);
	assert(out.b[3] == 2.0);

	// Second contribution must accumulate, not assign.
	assert(Nodal::apply_endpoint_condensation(out, agg, 0.25, 0));
	assert(out.b[0] == 0.75);

	assert(!Nodal::apply_endpoint_condensation(out, agg, 0.5, 2));
	assert(!Nodal::apply_endpoint_condensation(out, agg, -0.5, 0));
	return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.s4b-condensation-writer.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "accumulation": True,
        "invalid_input_rejected": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="s4b-cond-", dir=ROOT / ".tmp") as directory:
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
    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-CONDENSE {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
