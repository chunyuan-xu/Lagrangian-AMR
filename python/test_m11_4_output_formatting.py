"""M11.4 micro-gate: output formatting and distance-profile initialization."""

import argparse
import json
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-m11-4-output-formatting.json"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.m11-4-output-formatting.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "half_time_format_extra_arg_removed": False,
        "distance_initialized": False,
        "unsupported_profile_aborts": False,
        "status": "FAIL",
    }

    hydro = (ROOT / "src/hydro/hydro_callbacks.h").read_text(encoding="utf-8")
    io = (ROOT / "src/io/io_callbacks.h").read_text(encoding="utf-8")

    if ('P4EST_GLOBAL_PRODUCTIONF("The half time internal energy is illegal\\n");'
            in hydro and
            'P4EST_GLOBAL_PRODUCTIONF("The half time pressure is illegal\\n");'
            in hydro):
        summary["half_time_format_extra_arg_removed"] = True

    if "double distance = 0.;" in io:
        summary["distance_initialized"] = True

    if ('"Unsupported distance profile type %d\\n"' in io and
            "std::abort();" in io):
        summary["unsupported_profile_aborts"] = True

    summary["status"] = (
        "PASS" if all([
            summary["half_time_format_extra_arg_removed"],
            summary["distance_initialized"],
            summary["unsupported_profile_aborts"],
        ]) else "FAIL"
    )
    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"MG-M11-4 {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
