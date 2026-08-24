"""Canonical serial GOLDEN regression entry point.

Runs Noh Uniform, Sod AMR, and Sedov AMR with the complete frozen GOLDEN
configuration, compares terminal VTU files at tolerance 1e-6, writes a
machine-readable JSON summary, and restores param.ini byte-for-byte.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

from gates_common import GATE_TOLERANCE

ROOT = Path(__file__).resolve().parent.parent
PARAM = ROOT / "param.ini"
SOLVER = ROOT / "bin" / "AMR_Solver.exe"
OUTPUT = ROOT / "output"
COMPARE_VTU = Path(__file__).resolve().parent / "compare_vtu.py"
SUMMARY = ROOT / "serial_golden_summary.json"
MSYS_PATHS = [
    "C:/msys64/usr/bin",
    "C:/msys64/ucrt64/bin",
    "C:/Program Files/Microsoft MPI/Bin",
]
GOLDEN_COMMON = {
    "refine_err": 1.0,
    "coarsen_error": 0.8,
    "refine_period": 4,
    "refine_coarsen_time": 0.0001,
    "write_interval_step": 200000,
    "max_time_step": 200000,
}
CASES = [
    {
        "name": "Noh Uniform",
        "which_case": 4,
        "end_time": 0.6,
        "enable_amr": "false",
        "minus_level": 5,
        "max_level": 5,
        "reference": "Noh_32x32.vtu",
    },
    {
        "name": "Sod AMR",
        "which_case": 7,
        "end_time": 0.2,
        "enable_amr": "true",
        "minus_level": 5,
        "max_level": 7,
        "reference": "SodAMR.vtu",
    },
    {
        "name": "Sedov AMR",
        "which_case": 1,
        "end_time": 0.5,
        "enable_amr": "true",
        "minus_level": 5,
        "max_level": 7,
        "reference": "SedovAMR.vtu",
    },
]


def solver_env(memory_high_water=False):
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join(MSYS_PATHS + [environment.get("PATH", "")])
    if memory_high_water:
        environment["LAGRANGIAN_MEMORY_HIGH_WATER"] = "1"
    else:
        environment.pop("LAGRANGIAN_MEMORY_HIGH_WATER", None)
    return environment


def update_param_text(text, updates):
    result = text
    for key, value in updates.items():
        pattern = rf"(?m)^(\s*{re.escape(key)}\s*=\s*)[^\r\n]*(\r?\n|$)"
        result, count = re.subn(pattern, rf"\g<1>{value}\g<2>", result, count=1)
        if count == 0:
            result = result.rstrip("\r\n") + f"\n{key} = {value}\n"
    return result


def reset_output():
    OUTPUT.mkdir(exist_ok=True)
    for path in OUTPUT.iterdir():
        if path.is_file():
            path.unlink()
        else:
            raise RuntimeError(f"Unexpected directory inside output/: {path}")


def terminal_vtu():
    outputs = sorted(OUTPUT.glob("p4est_Lagrangian_*_0000.vtu"))
    if not outputs:
        raise FileNotFoundError("No serial VTU output generated")
    return outputs[-1]


def run_case(case, original_param, memory_high_water=False):
    updates = {
        "which_case": case["which_case"],
        "end_time": case["end_time"],
        "enable_amr": case["enable_amr"],
        "minus_level": case["minus_level"],
        "max_level": case["max_level"],
        "refine_coarsen_enum": 0,
        "write_interval_time": case["end_time"],
        **GOLDEN_COMMON,
    }
    PARAM.write_text(update_param_text(original_param, updates), encoding="utf-8", newline="")
    reset_output()

    print(f"\n{'=' * 50}\nRunning {case['name']}\n{'=' * 50}", flush=True)
    started = time.perf_counter()
    solver = subprocess.run(
        [str(SOLVER)], cwd=ROOT, env=solver_env(memory_high_water), capture_output=True, text=True
    )
    elapsed = time.perf_counter() - started
    result = {
        "name": case["name"],
        "reference": case["reference"],
        "seconds": round(elapsed, 3),
        "solver_exit_code": solver.returncode,
        "compare_exit_code": None,
        "status": "FAIL",
    }
    print(f"[solve] {case['name']} took {elapsed:.1f}s", flush=True)
    if solver.returncode != 0:
        result["failure"] = "solver"
        result["stderr_tail"] = solver.stderr[-1000:]
        return result

    target = terminal_vtu()
    compare = subprocess.run(
        [
            sys.executable,
            str(COMPARE_VTU),
            "--target",
            str(target),
            "--ref",
            str(ROOT / "reference" / case["reference"]),
            "--tol",
            str(GATE_TOLERANCE),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    result["target"] = str(target.relative_to(ROOT))
    result["compare_exit_code"] = compare.returncode
    if compare.returncode != 0:
        result["failure"] = "comparison"
        result["comparison_output"] = compare.stdout + compare.stderr
        return result

    result["status"] = "PASS"
    print(f"PASS: {case['name']} -> {result['target']}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--memory-high-water",
        action="store_true",
        help="enable the external memory high-water observation path",
    )
    args = parser.parse_args()

    if not SOLVER.exists():
        print(f"ERROR: solver not found: {SOLVER}", file=sys.stderr)
        return 2

    original_bytes = PARAM.read_bytes()
    original_param = original_bytes.decode("utf-8")
    results = []
    started_at = datetime.now().astimezone().isoformat()
    exit_code = 1
    try:
        for case in CASES:
            result = run_case(case, original_param, args.memory_high_water)
            results.append(result)
            if result["status"] != "PASS":
                print(f"FAIL: {case['name']} ({result.get('failure')})", file=sys.stderr)
                break
        exit_code = 0 if len(results) == len(CASES) and all(
            result["status"] == "PASS" for result in results
        ) else 1
    except Exception as error:
        results.append({"name": "runner", "status": "ERROR", "error": str(error)})
        print(f"ERROR: {error}", file=sys.stderr)
        exit_code = 2
    finally:
        PARAM.write_bytes(original_bytes)

    summary = {
        "schema": "lagrangian-amr.serial-golden.v1",
        "started_at": started_at,
        "tolerance": GATE_TOLERANCE,
        "golden_common": GOLDEN_COMMON,
        "memory_high_water": args.memory_high_water,
        "param_restored": PARAM.read_bytes() == original_bytes,
        "status": "PASS" if exit_code == 0 else "FAIL",
        "cases": results,
    }
    SUMMARY.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print("\n==== SERIAL GOLDEN SUMMARY ====")
    for result in results:
        print(f"{result['name']}: {result['status']}")
    print(f"param.ini restored: {summary['param_restored']}")
    print(f"machine summary: {SUMMARY}")
    return exit_code if summary["param_restored"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
