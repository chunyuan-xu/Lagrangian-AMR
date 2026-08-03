"""Canonical MPI regression gates.

G2 checks serial/MPI consistency at steps 3, 4, 10, 50, and 54 using the
stable geometry key workflow. G3 regenerates 4-rank Sod/Sedov GOLDEN outputs
and compares them with the committed parallel references.
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

ROOT = Path(__file__).resolve().parent.parent
PARAM = ROOT / "param.ini"
SOLVER = ROOT / "bin" / "AMR_Solver.exe"
OUTPUT = ROOT / "output"
MPIEXEC = Path("C:/Program Files/Microsoft MPI/Bin/mpiexec.exe")
QUICK = Path(__file__).resolve().parent / "quick_consistency_test.py"
COMPARE = Path(__file__).resolve().parent / "compare_vtu.py"
SUMMARY = ROOT / "mpi_gate_summary.json"
STEPS = [3, 4, 10, 50, 54]
MSYS_PATHS = [
    "C:/msys64/usr/bin",
    "C:/msys64/ucrt64/bin",
    str(MPIEXEC.parent),
]
PARALLEL_CASES = [
    {"name": "Sod AMR", "which_case": 7, "end_time": 0.2, "step": 3046,
     "reference": ROOT / "reference" / "par4_sod" / "p4est_Lagrangian_3046.pvtu"},
    {"name": "Sedov AMR", "which_case": 1, "end_time": 0.5, "step": 3933,
     "reference": ROOT / "reference" / "par4_sedov" / "p4est_Lagrangian_3933.pvtu"},
]


def environment():
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(MSYS_PATHS + [env.get("PATH", "")])
    for key in (
        "LAGRANGIAN_TRACE_TARGET",
        "LAGRANGIAN_TRACE_REFINE",
        "LAGRANGIAN_VERBOSE_AMR",
        "LAGRANGIAN_TRACE_CHECKSUM",
    ):
        env.pop(key, None)
    return env


def update_text(text, updates):
    result = text
    for key, value in updates.items():
        pattern = rf"(?m)^(\s*{re.escape(key)}\s*=\s*)[^\r\n]*(\r?\n|$)"
        result, count = re.subn(pattern, rf"\g<1>{value}\g<2>", result, count=1)
        if count == 0:
            result = result.rstrip("\r\n") + f"\n{key} = {value}\n"
    return result


def ensure_output():
    OUTPUT.mkdir(exist_ok=True)


def run_g2():
    results = []
    for step in STEPS:
        started = time.perf_counter()
        command = [sys.executable, str(QUICK), "--step", str(step), "--ranks", "2", "--tol", "1e-12"]
        result = subprocess.run(command, cwd=ROOT, env=environment(), capture_output=True, text=True)
        attempts = 1
        combined = result.stdout + result.stderr
        # A Windows abort/crash is an infrastructure-level process failure. Retry it
        # once; deterministic topology/field failures return code 1 and are never retried.
        if result.returncode not in (0, 1):
            attempts = 2
            result = subprocess.run(command, cwd=ROOT, env=environment(), capture_output=True, text=True)
            combined += "\n--- RETRY ---\n" + result.stdout + result.stderr
        item = {
            "step": step,
            "exit_code": result.returncode,
            "attempts": attempts,
            "seconds": round(time.perf_counter() - started, 3),
            "status": "PASS" if result.returncode == 0 else "FAIL",
        }
        if result.returncode != 0:
            item["output_tail"] = combined[-2000:]
        results.append(item)
        print(f"G2 step {step}: {item['status']} ({item['seconds']:.1f}s)", flush=True)
        if result.returncode != 0:
            break
    return results


def run_g3(original_text):
    results = []
    common = {
        "enable_amr": "true", "minus_level": 5, "max_level": 7,
        "refine_coarsen_enum": 0, "refine_err": 1.0, "coarsen_error": 0.8,
        "refine_period": 4, "refine_coarsen_time": 0.0001,
        "write_interval_step": 200000, "max_time_step": 200000,
    }
    for case in PARALLEL_CASES:
        updates = {**common, "which_case": case["which_case"], "end_time": case["end_time"],
                   "write_interval_time": case["end_time"]}
        PARAM.write_text(update_text(original_text, updates), encoding="utf-8", newline="")
        ensure_output()
        started = time.perf_counter()
        solver = subprocess.run(
            [str(MPIEXEC), "-n", "4", str(SOLVER)], cwd=ROOT, env=environment(),
            capture_output=True, text=True,
        )
        item = {
            "name": case["name"], "solver_exit_code": solver.returncode,
            "seconds": round(time.perf_counter() - started, 3), "status": "FAIL",
        }
        target = OUTPUT / f"p4est_Lagrangian_{case['step']:04d}.pvtu"
        if solver.returncode == 0 and target.exists():
            compare = subprocess.run(
                [sys.executable, str(COMPARE), "--target", str(target), "--ref", str(case["reference"]), "--tol", "1e-12"],
                cwd=ROOT, capture_output=True, text=True,
            )
            item["compare_exit_code"] = compare.returncode
            if compare.returncode == 0:
                item["status"] = "PASS"
            else:
                item["comparison_tail"] = (compare.stdout + compare.stderr)[-2000:]
        else:
            item["solver_tail"] = (solver.stdout + solver.stderr)[-2000:]
        results.append(item)
        print(f"G3 {case['name']}: {item['status']} ({item['seconds']:.1f}s)", flush=True)
        if item["status"] != "PASS":
            break
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", choices=("g2", "g3", "all"), default="all")
    args = parser.parse_args()
    if not SOLVER.exists() or not MPIEXEC.exists():
        print("ERROR: solver or mpiexec missing", file=sys.stderr)
        return 2

    original_bytes = PARAM.read_bytes()
    original_text = original_bytes.decode("utf-8")
    g2_results = []
    g3_results = []
    status = "FAIL"
    try:
        if args.gate in ("g2", "all"):
            g2_results = run_g2()
            if not all(item["status"] == "PASS" for item in g2_results) or len(g2_results) != len(STEPS):
                return_code = 1
            else:
                return_code = 0
        else:
            return_code = 0
        if return_code == 0 and args.gate in ("g3", "all"):
            g3_results = run_g3(original_text)
            if not all(item["status"] == "PASS" for item in g3_results) or len(g3_results) != len(PARALLEL_CASES):
                return_code = 1
        status = "PASS" if return_code == 0 else "FAIL"
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return_code = 2
        status = "ERROR"
        g3_results.append({"status": "ERROR", "error": str(error)})
    finally:
        PARAM.write_bytes(original_bytes)

    summary = {
        "schema": "lagrangian-amr.mpi-gates.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "requested_gate": args.gate,
        "status": status,
        "param_restored": PARAM.read_bytes() == original_bytes,
        "g2": g2_results,
        "g3": g3_results,
    }
    SUMMARY.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"MPI gates: {status}; summary: {SUMMARY}")
    return return_code if summary["param_restored"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
