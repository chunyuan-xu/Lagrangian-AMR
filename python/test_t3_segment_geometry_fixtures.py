"""T3a micro-gate: analytic regular/split segment geometry fixtures."""

import argparse
import json
import math
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t3a-segment-geometry.json"

POSITIVE = [
    {"name": "regular_axis_x", "valid": True, "kind": "regular",
     "a": (0.0, 0.0), "b": (1.0, 0.0),
     "normal": (0.0, 1.0), "length": 1.0, "weights": (0.5, 0.5)},
    {"name": "regular_axis_y", "valid": True, "kind": "regular",
     "a": (0.0, 0.0), "b": (0.0, 1.0),
     "normal": (-1.0, 0.0), "length": 1.0, "weights": (0.5, 0.5)},
    {"name": "split_horizontal", "valid": True, "kind": "split",
     "a": (0.0, 0.0), "m": (0.5, 0.0), "b": (1.0, 0.0),
     "segment_lengths": (0.5, 0.5),
     "segment_weights": ((0.5, 0.5), (0.5, 0.5))},
]

NEGATIVE = [
    {"name": "zero_length", "valid": False, "kind": "regular",
     "a": (0.0, 0.0), "b": (0.0, 0.0),
     "reason": "zero length"},
    {"name": "non_unit_normal", "valid": False, "kind": "regular",
     "a": (0.0, 0.0), "b": (2.0, 0.0),
     "normal": (0.0, 2.0), "length": 2.0, "weights": (0.5, 0.5),
     "reason": "normal not unit"},
    {"name": "negative_weight", "valid": False, "kind": "regular",
     "a": (0.0, 0.0), "b": (1.0, 0.0),
     "normal": (0.0, 1.0), "length": 1.0, "weights": (-0.5, 1.5),
     "reason": "negative weight"},
    {"name": "weights_not_sum_one", "valid": False, "kind": "regular",
     "a": (0.0, 0.0), "b": (1.0, 0.0),
     "normal": (0.0, 1.0), "length": 1.0, "weights": (0.4, 0.4),
     "reason": "weights sum != 1"},
    {"name": "split_length_mismatch", "valid": False, "kind": "split",
     "a": (0.0, 0.0), "m": (0.5, 0.0), "b": (1.0, 0.0),
     "segment_lengths": (0.7, 0.5),
     "reason": "split lengths do not add to full length"},
]


def validate_fixture(fixture):
    kind = fixture.get("kind")
    if kind == "regular":
        a, b = fixture["a"], fixture["b"]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        if length <= 1e-12:
            return False, "zero length"
        if abs(fixture["length"] - length) > 1e-12:
            return False, "length mismatch"
        n = fixture["normal"]
        if abs(math.hypot(n[0], n[1]) - 1.0) > 1e-12:
            return False, "non unit normal"
        w = fixture["weights"]
        if w[0] < 0 or w[1] < 0:
            return False, "negative weight"
        if abs(w[0] + w[1] - 1.0) > 1e-12:
            return False, "weights sum != 1"
        return True, None
    if kind == "split":
        a, m, b = fixture["a"], fixture["m"], fixture["b"]
        full = math.hypot(b[0] - a[0], b[1] - a[1])
        s1 = math.hypot(m[0] - a[0], m[1] - a[1])
        s2 = math.hypot(b[0] - m[0], b[1] - m[1])
        if abs(s1 + s2 - full) > 1e-12:
            return False, "split lengths do not add to full length"
        if abs(fixture["segment_lengths"][0] - s1) > 1e-12 or \
           abs(fixture["segment_lengths"][1] - s2) > 1e-12:
            return False, "split segment length mismatch"
        for weights in fixture["segment_weights"]:
            if weights[0] < 0 or weights[1] < 0 or abs(weights[0] + weights[1] - 1.0) > 1e-12:
                return False, "bad split endpoint weights"
        return True, None
    return False, "unknown kind"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    fixtures = POSITIVE + NEGATIVE
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
        "schema": "lagrangian-amr.micro-gate.t3a-segment-geometry.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "fixture_count": len(fixtures),
        "positive_count": len(POSITIVE),
        "negative_count": len(NEGATIVE),
        "failures": failures,
        "status": "PASS" if not failures else "FAIL",
    }
    args.summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8")
    print(f"MG-T3A {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())