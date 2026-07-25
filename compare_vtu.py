#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VTU Output Comparison Script for Regression Testing.
Compares numerical field outputs (density, pressure, etc.) between a newly generated VTU
and a reference baseline VTU file.
"""

import os
import sys
import argparse
import base64
import xml.etree.ElementTree as ET
import numpy as np

def parse_vtu_file(vtu_path):
    """
    Parses a VTU file (VTK UnstructuredGrid XML) and extracts DataArray contents into numpy arrays.
    Returns:
        dict: {
            'num_points': int,
            'num_cells': int,
            'data': { name: np.ndarray }
        }
    """
    if not os.path.exists(vtu_path):
        raise FileNotFoundError(f"VTU file not found: {vtu_path}")

    tree = ET.parse(vtu_path)
    root = tree.getroot()
    piece = root.find('.//Piece')
    if piece is None:
        raise ValueError(f"Invalid VTU format: missing <Piece> element in {vtu_path}")

    num_points = int(piece.attrib.get('NumberOfPoints', 0))
    num_cells = int(piece.attrib.get('NumberOfCells', 0))

    data_arrays = {}
    for da in root.findall('.//DataArray'):
        name = da.attrib.get('Name')
        dtype_str = da.attrib.get('type')
        fmt = da.attrib.get('format', 'ascii')
        raw_text = (da.text or '').strip()

        if not name:
            continue

        if fmt == 'binary' and raw_text:
            raw_bytes = base64.b64decode(raw_text)
            # VTK XML binary headers start with 4-byte or 8-byte uint representing byte length
            header_len = np.frombuffer(raw_bytes[:4], dtype=np.uint32)[0]
            
            if dtype_str == 'Float32':
                dtype = np.float32
            elif dtype_str == 'Float64':
                dtype = np.float64
            elif dtype_str == 'Int32':
                dtype = np.int32
            elif dtype_str == 'Int64':
                dtype = np.int64
            elif dtype_str in ('UInt8', 'UnsignedChar'):
                dtype = np.uint8
            else:
                dtype = np.float32

            array_data = np.frombuffer(raw_bytes[4:4+header_len], dtype=dtype)
        else:
            # ASCII format fallback
            if dtype_str in ('Float32', 'Float64'):
                array_data = np.fromstring(raw_text, sep=' ', dtype=np.float64)
            else:
                array_data = np.fromstring(raw_text, sep=' ', dtype=np.int64)

        data_arrays[name] = array_data

    return {
        'num_points': num_points,
        'num_cells': num_cells,
        'data': data_arrays
    }


def compare_vtu(target_path, ref_path, tol=1e-10, fields_to_check=('density', 'Pressure')):
    """
    Compares target_path against ref_path.
    Returns True if passed, False otherwise.
    """
    print("=" * 65)
    print(f" VTU Regression Comparison Test ")
    print("=" * 65)
    print(f"Target File : {target_path}")
    print(f"Ref File    : {ref_path}")
    print(f"Tolerance   : {tol:.1e}")
    print("-" * 65)

    try:
        target_info = parse_vtu_file(target_path)
        ref_info = parse_vtu_file(ref_path)
    except Exception as e:
        print(f"[ERROR] Failed to parse files: {e}")
        return False

    # Check mesh topology sizes
    mesh_pass = True
    if target_info['num_points'] != ref_info['num_points']:
        print(f"[FAIL] NumberOfPoints mismatch: Target={target_info['num_points']} vs Ref={ref_info['num_points']}")
        mesh_pass = False
    if target_info['num_cells'] != ref_info['num_cells']:
        print(f"[FAIL] NumberOfCells mismatch: Target={target_info['num_cells']} vs Ref={ref_info['num_cells']}")
        mesh_pass = False

    if not mesh_pass:
        return False

    print(f"Mesh Structure Check : PASSED (Points: {target_info['num_points']}, Cells: {target_info['num_cells']})")
    print("-" * 65)

    # Detailed field comparison
    all_passed = True
    print(f"{'Field Name':<18} | {'Max Abs Diff':<14} | {'Rel Diff':<14} | {'Status'}")
    print("-" * 65)

    check_fields = list(fields_to_check)
    # Also check additional fields if present in both
    for extra in ['internal_energy', 'NodeX', 'NodeY', 'NodeU', 'NodeV', 'Position']:
        if extra in target_info['data'] and extra in ref_info['data'] and extra not in check_fields:
            check_fields.append(extra)

    for field in check_fields:
        if field not in target_info['data']:
            print(f"{field:<18} | {'MISSING IN TARGET':<31} | [FAIL]")
            all_passed = False
            continue
        if field not in ref_info['data']:
            print(f"{field:<18} | {'MISSING IN REF':<31} | [FAIL]")
            all_passed = False
            continue

        arr_target = target_info['data'][field].astype(np.float64)
        arr_ref = ref_info['data'][field].astype(np.float64)

        if arr_target.shape != arr_ref.shape:
            print(f"{field:<18} | {'SHAPE MISMATCH':<31} | [FAIL]")
            all_passed = False
            continue

        abs_diff = np.abs(arr_target - arr_ref)
        max_abs_diff = np.max(abs_diff)
        
        max_val = np.max(np.abs(arr_ref))
        rel_diff = max_abs_diff / (max_val + 1e-15)

        passed = max_abs_diff <= tol

        if not passed:
            all_passed = False
            status_str = "FAIL"
        else:
            status_str = "PASS"

        print(f"{field:<18} | {max_abs_diff:<14.6e} | {rel_diff:<14.6e} | [{status_str}]")

        if not passed:
            max_idx = np.argmax(abs_diff)
            print(f"   └─ Max discrepancy at index {max_idx}: Target={arr_target[max_idx]:.10e}, Ref={arr_ref[max_idx]:.10e}")

    print("=" * 65)
    if all_passed:
        print(f"[SUCCESS] All checked fields differ by less than tolerance ({tol:.1e}).")
        return True
    else:
        print(f"[FAILURE] Discrepancies detected exceeding tolerance ({tol:.1e}).")
        return False


def main():
    parser = argparse.ArgumentParser(description="Compare VTU files for regression testing.")
    parser.add_argument("--target", type=str, default="bin/output/p4est_Lagrangian_1000_0000.vtu",
                        help="Path to newly generated target VTU file")
    parser.add_argument("--ref", type=str, default="reference/SodAMR.vtu",
                        help="Path to reference VTU file")
    parser.add_argument("--tol", type=float, default=1e-10,
                        help="Tolerance threshold for floating point field comparison (default: 1e-10)")

    args = parser.parse_args()

    success = compare_vtu(args.target, args.ref, tol=args.tol)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
