#!/usr/bin/env python3
"""Compare serial and MPI VTU outputs by stable undeformed cell geometry."""

import base64
import math
import os
import struct
import sys
import xml.etree.ElementTree as ET

TYPE_FORMATS = {
    "Float32": ("f", 4),
    "Float64": ("d", 8),
    "Int32": ("i", 4),
    "Int64": ("q", 8),
    "UInt8": ("B", 1),
    "UnsignedChar": ("B", 1),
}


def decode_array(element):
    dtype = element.attrib.get("type", "Float32")
    fmt = element.attrib.get("format", "ascii")
    text = (element.text or "").strip()
    if fmt == "ascii":
        converter = float if dtype.startswith("Float") else int
        return [converter(value) for value in text.split()]
    raw = base64.b64decode(text)
    if len(raw) < 4:
        return []
    byte_count = struct.unpack_from("<I", raw, 0)[0]
    code, item_size = TYPE_FORMATS[dtype]
    count = byte_count // item_size
    return list(struct.unpack_from(f"<{count}{code}", raw, 4))


def parse_vtu(path):
    root = ET.parse(path).getroot()
    piece = root.find(".//Piece")
    if piece is None:
        raise ValueError(f"Missing Piece in {path}")
    arrays = {}
    components = {}
    for element in root.findall(".//DataArray"):
        name = element.attrib.get("Name")
        if not name:
            continue
        arrays[name] = decode_array(element)
        components[name] = int(element.attrib.get("NumberOfComponents", "1"))
    time_element = root.find(".//FieldData/DataArray[@Name='TimeValue']")
    time_value = None
    if time_element is not None:
        values = decode_array(time_element)
        time_value = values[0] if values else None
    return {
        "path": path,
        "num_points": int(piece.attrib["NumberOfPoints"]),
        "num_cells": int(piece.attrib["NumberOfCells"]),
        "arrays": arrays,
        "components": components,
        "time": time_value,
    }


def parse_pvtu(path):
    root = ET.parse(path).getroot()
    base = os.path.dirname(path)
    pieces = [parse_vtu(os.path.join(base, item.attrib["Source"]))
              for item in root.findall(".//PUnstructuredGrid/Piece")]
    time_element = root.find(".//FieldData/DataArray[@Name='TimeValue']")
    time_value = None
    if time_element is not None:
        values = decode_array(time_element)
        time_value = values[0] if values else None
    common_names = set.intersection(*(set(piece["arrays"]) for piece in pieces))
    arrays = {name: sum((piece["arrays"][name] for piece in pieces), [])
              for name in common_names}
    return {
        "path": path,
        "num_points": sum(piece["num_points"] for piece in pieces),
        "num_cells": sum(piece["num_cells"] for piece in pieces),
        "arrays": arrays,
        "components": pieces[0]["components"],
        "time": time_value,
        "piece_sizes": [(piece["num_cells"], piece["num_points"]) for piece in pieces],
    }


def cell_key(position, cell):
    start = 12 * cell
    points = [tuple(position[start + 3 * corner:start + 3 * corner + 3])
              for corner in range(4)]
    return tuple(sorted(points))


def cell_description(key):
    xs = [point[0] for point in key]
    ys = [point[1] for point in key]
    return (min(xs), min(ys), max(xs), max(ys))


def build_index(dataset):
    positions = dataset["arrays"]["Position"]
    expected = dataset["num_cells"] * 12
    if len(positions) != expected:
        raise ValueError(f"Position length {len(positions)} != {expected}")
    index = {}
    duplicates = []
    for cell in range(dataset["num_cells"]):
        key = cell_key(positions, cell)
        if key in index:
            duplicates.append((key, index[key], cell))
        index[key] = cell
    return index, duplicates


def compare_field(serial, mpi, serial_index, mpi_index, common_keys, field, width):
    serial_values = serial["arrays"][field]
    mpi_values = mpi["arrays"][field]
    maximum = -1.0
    worst = None
    mismatch_count = 0
    exact_count = 0
    for key in common_keys:
        serial_cell = serial_index[key]
        mpi_cell = mpi_index[key]
        for component in range(width):
            left = serial_values[width * serial_cell + component]
            right = mpi_values[width * mpi_cell + component]
            difference = abs(left - right)
            if difference == 0.0:
                exact_count += 1
            if difference > 1.0e-10:
                mismatch_count += 1
            if difference > maximum:
                maximum = difference
                worst = (key, component, left, right)
    return maximum, mismatch_count, exact_count, worst


def format_cell(key):
    x0, y0, x1, y1 = cell_description(key)
    return f"bbox=({x0:.9g}, {y0:.9g})-({x1:.9g}, {y1:.9g}), size=({x1-x0:.9g}, {y1-y0:.9g})"


def main():
    if len(sys.argv) != 3:
        print("usage: compare_100_by_geometry.py SERIAL.vtu MPI.pvtu", file=sys.stderr)
        return 2
    serial = parse_vtu(sys.argv[1])
    mpi = parse_pvtu(sys.argv[2])
    serial_index, serial_duplicates = build_index(serial)
    mpi_index, mpi_duplicates = build_index(mpi)
    serial_keys = set(serial_index)
    mpi_keys = set(mpi_index)
    common_keys = sorted(serial_keys & mpi_keys)
    serial_only = sorted(serial_keys - mpi_keys)
    mpi_only = sorted(mpi_keys - serial_keys)

    print("100-step serial/MPI comparison by undeformed cell geometry")
    print("=" * 72)
    print(f"Serial cells/points : {serial['num_cells']} / {serial['num_points']}")
    print(f"MPI cells/points    : {mpi['num_cells']} / {mpi['num_points']}")
    print(f"MPI piece sizes     : {mpi.get('piece_sizes')}")
    serial_time = "not written" if serial["time"] is None else f"{serial['time']:.17g}"
    mpi_time = "not written" if mpi["time"] is None else f"{mpi['time']:.17g}"
    print(f"Serial TimeValue    : {serial_time}")
    print(f"MPI TimeValue       : {mpi_time}")
    if serial["time"] is not None and mpi["time"] is not None:
        print(f"TimeValue abs diff  : {abs(serial['time'] - mpi['time']):.9e}")
    else:
        print("TimeValue abs diff  : unavailable")
    print(f"Common cells        : {len(common_keys)}")
    print(f"Serial-only cells   : {len(serial_only)}")
    print(f"MPI-only cells      : {len(mpi_only)}")
    print(f"Duplicate geometry  : serial={len(serial_duplicates)}, mpi={len(mpi_duplicates)}")
    print()

    print("Fields on common cells (threshold 1e-10; VTU values are Float32)")
    print("-" * 72)
    fields = [
        ("NodeU", 4),
        ("NodeV", 4),
        ("NodeX", 4),
        ("NodeY", 4),
        ("Pressure", 1),
        ("density", 1),
        ("internal_energy", 1),
    ]
    for field, width in fields:
        maximum, mismatches, exact, worst = compare_field(
            serial, mpi, serial_index, mpi_index, common_keys, field, width)
        total = len(common_keys) * width
        status = "PASS" if mismatches == 0 else "FAIL"
        print(f"{field:18s} max_abs={maximum:.9e}  >tol={mismatches:6d}/{total:<6d} exact={exact:6d}  {status}")
        if worst is not None and maximum > 0.0:
            key, component, left, right = worst
            label = f"corner={component}" if width == 4 else "cell scalar"
            print(f"  worst: {format_cell(key)}, {label}, serial={left:.9e}, mpi={right:.9e}")

    print()
    print("Serial-only cell geometries")
    print("-" * 72)
    for key in serial_only:
        print(format_cell(key))
    print()
    print("MPI-only cell geometries")
    print("-" * 72)
    for key in mpi_only:
        print(format_cell(key))

    topology_equal = not serial_only and not mpi_only
    return 0 if topology_equal else 1


if __name__ == "__main__":
    raise SystemExit(main())
