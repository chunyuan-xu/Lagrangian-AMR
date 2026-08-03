#!/usr/bin/env python3
"""Run a serial/MPI consistency check at one requested time step.

The workflow always restores param.ini, isolates outputs in a timestamped directory,
and stops before field comparison when serial/MPI cell counts differ.
"""

import argparse
import contextlib
import datetime
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from compare_100_by_geometry import (
    build_index,
    compare_field,
    format_cell,
    parse_pvtu,
    parse_vtu,
)

# Project root = parent of python/ (where param.ini, bin/, reference/ live).
ROOT = Path(__file__).resolve().parent.parent
PARAM = ROOT / "param.ini"
SOLVER = ROOT / "bin" / "AMR_Solver.exe"
OUTPUT = ROOT / "output"
MPIEXEC = Path("C:/Program Files/Microsoft MPI/Bin/mpiexec.exe")
MSYS_BIN = Path("C:/msys64/usr/bin")
UCRT_BIN = Path("C:/msys64/ucrt64/bin")
FIELDS = [
    ("NodeU", 4),
    ("NodeV", 4),
    ("NodeX", 4),
    ("NodeY", 4),
    ("Pressure", 1),
    ("density", 1),
    ("internal_energy", 1),
]


def replace_ini_value(text, key, value):
    pattern = rf"(?m)^(\s*{re.escape(key)}\s*=\s*)[^\r\n]*(\r?\n|$)"
    updated, count = re.subn(pattern, rf"\g<1>{value}\g<2>", text, count=1)
    if count != 1:
        raise ValueError(f"Cannot find unique parameter: {key}")
    return updated


def run_command(command, log_path, environment):
    OUTPUT.mkdir(exist_ok=True)
    with log_path.open("w", encoding="utf-8", newline="") as log:
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
        )
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed with exit code {result.returncode}; see {log_path}"
        )


def copy_serial_output(step, destination):
    source = OUTPUT / f"p4est_Lagrangian_{step:04d}_0000.vtu"
    if not source.exists():
        raise FileNotFoundError(f"Missing serial output: {source}")
    shutil.copy2(source, destination / source.name)
    return destination / source.name


def copy_mpi_output(step, ranks, destination):
    pvtu = OUTPUT / f"p4est_Lagrangian_{step:04d}.pvtu"
    if not pvtu.exists():
        raise FileNotFoundError(f"Missing MPI output: {pvtu}")
    shutil.copy2(pvtu, destination / pvtu.name)
    for rank in range(ranks):
        piece = OUTPUT / f"p4est_Lagrangian_{step:04d}_{rank:04d}.vtu"
        if not piece.exists():
            raise FileNotFoundError(f"Missing MPI piece: {piece}")
        shutil.copy2(piece, destination / piece.name)
    return destination / pvtu.name


def compare_datasets(serial, mpi, tolerance):
    serial_index, serial_duplicates = build_index(serial)
    mpi_index, mpi_duplicates = build_index(mpi)
    serial_keys = set(serial_index)
    mpi_keys = set(mpi_index)
    common = sorted(serial_keys & mpi_keys)
    serial_only = sorted(serial_keys - mpi_keys)
    mpi_only = sorted(mpi_keys - serial_keys)

    lines = [
        f"Common cells        : {len(common)}",
        f"Serial-only cells   : {len(serial_only)}",
        f"MPI-only cells      : {len(mpi_only)}",
        f"Duplicate geometry  : serial={len(serial_duplicates)}, mpi={len(mpi_duplicates)}",
    ]
    topology_equal = (
        not serial_only
        and not mpi_only
        and not serial_duplicates
        and not mpi_duplicates
    )
    if not topology_equal:
        lines.append("STOP: geometry sets differ; field comparison skipped.")
        return False, lines

    all_fields_equal = True
    lines.extend(["", f"Fields on common cells (tolerance={tolerance:.1e})"])
    for field, width in FIELDS:
        maximum, mismatches, exact, worst = compare_field(
            serial, mpi, serial_index, mpi_index, common, field, width
        )
        # compare_field uses 1e-10 internally for mismatch_count. Recompute only
        # the pass decision from max_abs so custom tolerance remains correct.
        passed = maximum <= tolerance
        all_fields_equal = all_fields_equal and passed
        total = len(common) * width
        lines.append(
            f"{field:18s} max_abs={maximum:.9e} exact={exact:6d}/{total:<6d} "
            f"{'PASS' if passed else 'FAIL'}"
        )
        if worst is not None and maximum > 0.0:
            key, component, serial_value, mpi_value = worst
            label = f"corner={component}" if width == 4 else "cell scalar"
            lines.append(
                f"  worst: {format_cell(key)}, {label}, "
                f"serial={serial_value:.9e}, mpi={mpi_value:.9e}"
            )
    return all_fields_equal, lines


def main():
    parser = argparse.ArgumentParser(
        description="Run serial/MPI consistency test at one time step."
    )
    parser.add_argument("--step", type=int, required=True, help="Target time step")
    parser.add_argument("--ranks", type=int, default=2, help="MPI ranks (default: 2)")
    parser.add_argument("--tol", type=float, default=1.0e-10)
    args = parser.parse_args()
    if args.step < 1:
        parser.error("--step must be >= 1")
    if args.ranks < 2:
        parser.error("--ranks must be >= 2")
    if not SOLVER.exists():
        raise FileNotFoundError(f"Solver not found: {SOLVER}")
    if not MPIEXEC.exists():
        raise FileNotFoundError(f"mpiexec not found: {MPIEXEC}")

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = ROOT / "step_tests" / f"step_{args.step:04d}" / timestamp
    serial_dir = run_dir / "serial"
    mpi_dir = run_dir / f"mpi{args.ranks}"
    serial_dir.mkdir(parents=True)
    mpi_dir.mkdir(parents=True)

    original_param = PARAM.read_text(encoding="utf-8")
    test_param = replace_ini_value(original_param, "max_time_step", args.step)
    test_param = replace_ini_value(test_param, "write_interval_step", args.step)
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join(
        [str(MSYS_BIN), str(UCRT_BIN), str(MPIEXEC.parent), environment.get("PATH", "")]
    )

    report_path = run_dir / "summary.txt"
    status = "ERROR"
    try:
        PARAM.write_text(test_param, encoding="utf-8", newline="")

        run_command([str(SOLVER)], run_dir / "serial.log", environment)
        serial_path = copy_serial_output(args.step, serial_dir)

        run_command(
            [str(MPIEXEC), "-n", str(args.ranks), str(SOLVER)],
            run_dir / f"mpi{args.ranks}.log",
            environment,
        )
        mpi_path = copy_mpi_output(args.step, args.ranks, mpi_dir)

        serial = parse_vtu(str(serial_path))
        mpi = parse_pvtu(str(mpi_path))
        lines = [
            f"Step             : {args.step}",
            f"MPI ranks        : {args.ranks}",
            f"Serial cells/pts : {serial['num_cells']} / {serial['num_points']}",
            f"MPI cells/pts    : {mpi['num_cells']} / {mpi['num_points']}",
            f"MPI piece sizes  : {mpi.get('piece_sizes')}",
            f"Result directory : {run_dir}",
            "",
        ]

        if serial["num_cells"] != mpi["num_cells"]:
            status = "STOP_CELL_COUNT_MISMATCH"
            lines.extend([
                "STOP: serial/MPI cell counts differ.",
                "Field comparison was not performed.",
            ])
        elif serial["num_points"] != mpi["num_points"]:
            status = "STOP_POINT_COUNT_MISMATCH"
            lines.extend([
                "STOP: cell counts match but point-entry counts differ.",
                "Field comparison was not performed.",
            ])
        else:
            equal, comparison_lines = compare_datasets(serial, mpi, args.tol)
            lines.extend(comparison_lines)
            status = "PASS" if equal else "FAIL_FIELDS_OR_GEOMETRY"

        lines.insert(0, f"Status           : {status}")
        report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("\n".join(lines))
        print(f"Summary          : {report_path}")
        return 0 if status == "PASS" else 1
    except Exception as error:
        report_path.write_text(
            f"Status           : ERROR\nError            : {error}\n",
            encoding="utf-8",
        )
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Summary: {report_path}", file=sys.stderr)
        return 2
    finally:
        PARAM.write_text(original_param, encoding="utf-8", newline="")


if __name__ == "__main__":
    raise SystemExit(main())
