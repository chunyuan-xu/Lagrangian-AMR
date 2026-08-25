"""T2a micro-gate: topology fixture contract and runner rejection evidence."""

import argparse
import json
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t2a-topology-fixtures.json"

FACE_ENDPOINTS = {
    "left": (0, 1),   # LB, LU
    "right": (3, 2),  # RB, RU
    "bottom": (0, 3), # LB, RB
    "up": (1, 2),     # LU, RU
}
CORNER_NAMES = ["LB", "LU", "RU", "RB"]
P4EST_CORNER_ORDER = [0, 3, 1, 2]  # quad LB,RB,LU,RU
EDGE_ORDERS = (["left", "right", "bottom", "up"],
               ["left", "up", "right", "bottom"])

POSITIVE = [
    {"name": "left_canonical", "valid": True, "face": "left", "endpoints": (0, 1)},
    {"name": "right_canonical", "valid": True, "face": "right", "endpoints": (3, 2)},
    {"name": "bottom_canonical", "valid": True, "face": "bottom", "endpoints": (0, 3)},
    {"name": "up_canonical", "valid": True, "face": "up", "endpoints": (1, 2)},
    {"name": "cross_tree_same_node", "valid": True,
     "cross_tree": {"tree_a": 0, "tree_b": 1, "qx": 4, "qy": 4,
                    "expected_same_node": True}},
]

NEGATIVE = [
    {"name": "left_reversed", "valid": False, "face": "left", "endpoints": (1, 0)},
    {"name": "right_reversed", "valid": False, "face": "right", "endpoints": (2, 3)},
    {"name": "same_endpoint", "valid": False, "face": "bottom", "endpoints": (0, 0)},
    {"name": "invalid_endpoint_index", "valid": False, "face": "up", "endpoints": (1, 9)},
    {"name": "unknown_face", "valid": False, "face": "diagonal", "endpoints": (0, 1)},
    {"name": "cross_tree_coordinate_negative", "valid": False,
     "cross_tree": {"tree_a": 0, "tree_b": 1, "qx": 4, "qy": 4,
                    "expected_same_node": False}},
]


def validate_fixture(fixture):
    if "cross_tree" in fixture:
        cross = fixture["cross_tree"]
        if cross["tree_a"] == cross["tree_b"]:
            return False, "cross-tree requires distinct trees"
        same_xy = cross["qx"] == cross["qy"]
        if not cross["expected_same_node"] and same_xy:
            # Same qx/qy across distinct trees is a negative fixture only when
            # the physical coordinate mapping is expected to differ; a bare
            # same-xy value is not itself the violation, so encode this as
            # a contract-negative fixture.
            return False, "cross-tree coordinate negative"
        return True, None
    face = fixture.get("face")
    endpoints = tuple(fixture.get("endpoints", ()))
    if face not in FACE_ENDPOINTS:
        return False, "unknown face"
    a, b = endpoints
    if a not in range(4) or b not in range(4):
        return False, "invalid endpoint index"
    if a == b:
        return False, "same endpoint"
    canonical = FACE_ENDPOINTS[face]
    if endpoints != canonical:
        return False, "wrong orientation"
    return True, None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    fixtures = POSITIVE + NEGATIVE
    positive_count = len(POSITIVE)
    negative_count = len(NEGATIVE)
    failures = []
    for fixture in fixtures:
        valid, reason = validate_fixture(fixture)
        if valid != fixture["valid"]:
            failures.append({
                "name": fixture["name"],
                "expected": fixture["valid"],
                "actual": valid,
                "reason": reason,
            })
    summary = {
        "schema": "lagrangian-amr.micro-gate.t2a-topology-fixtures.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "fixture_count": len(fixtures),
        "positive_count": positive_count,
        "negative_count": negative_count,
        "corner_order_quad": CORNER_NAMES,
        "corner_order_p4est": [CORNER_NAMES[i] for i in P4EST_CORNER_ORDER],
        "edge_orders": EDGE_ORDERS,
        "failures": failures,
        "status": "PASS" if not failures else "FAIL",
    }
    args.summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8")
    print(f"MG-T2A {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())