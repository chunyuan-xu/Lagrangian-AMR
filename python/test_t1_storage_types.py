"""T1b micro-gate: MG-LAYOUT and MG-RESET for scalar POD nodal storage."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t1b-layout-reset.json"

SOURCE = r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "nodal/nodal_storage.h"

template <typename T>
struct RawEligible : std::integral_constant<bool,
    std::is_standard_layout<T>::value &&
    std::is_trivially_copyable<T>::value &&
    std::is_trivially_destructible<T>::value> {};

static_assert(sizeof(Nodal::Vec2Storage) == 16, "Vec2Storage size");
static_assert(sizeof(Nodal::Mat2Storage) == 32, "Mat2Storage size");
static_assert(sizeof(Nodal::StageStamp) == 24, "StageStamp size");
static_assert(sizeof(Nodal::EdgeSegmentGeometry) == 40, "EdgeSegmentGeometry size");
static_assert(sizeof(Nodal::FaceData) == 88, "FaceData size");
static_assert(sizeof(Nodal::CellMasterContribution) == 192, "master size");
static_assert(sizeof(Nodal::CellHangingContribution) == 192, "hanging size");
static_assert(sizeof(Nodal::AggregatedHangingContribution) == 192, "aggregated size");
static_assert(sizeof(Nodal::CondensedMasterContribution) == 192, "condensed size");
static_assert(sizeof(Nodal::MasterSolveState) == 64, "solve state size");
static_assert(sizeof(Nodal::EvaluatedCellFlux) == 160, "evaluated flux size");
static_assert(sizeof(Nodal::CellNodalData) == 1176, "CellNodalData size");

static_assert(RawEligible<Nodal::Vec2Storage>::value, "Vec2Storage raw");
static_assert(RawEligible<Nodal::Mat2Storage>::value, "Mat2Storage raw");
static_assert(RawEligible<Nodal::StageStamp>::value, "StageStamp raw");
static_assert(RawEligible<Nodal::EdgeSegmentGeometry>::value, "segment raw");
static_assert(RawEligible<Nodal::FaceData>::value, "FaceData raw");
static_assert(RawEligible<Nodal::CellMasterContribution>::value, "master raw");
static_assert(RawEligible<Nodal::CellHangingContribution>::value, "hanging raw");
static_assert(RawEligible<Nodal::AggregatedHangingContribution>::value, "aggregated raw");
static_assert(RawEligible<Nodal::CondensedMasterContribution>::value, "condensed raw");
static_assert(RawEligible<Nodal::MasterSolveState>::value, "solve state raw");
static_assert(RawEligible<Nodal::EvaluatedCellFlux>::value, "evaluated raw");
static_assert(RawEligible<Nodal::CellNodalData>::value, "CellNodalData raw");

static_assert(std::is_same<decltype(Nodal::FaceData::flags), std::uint8_t>::value,
    "wire-visible flags must be uint8_t");
static_assert(!std::is_convertible<Nodal::CellHangingContribution,
    Nodal::AggregatedHangingContribution>::value, "hanging->aggregated convertible");
static_assert(!std::is_convertible<Nodal::AggregatedHangingContribution,
    Nodal::CondensedMasterContribution>::value, "aggregated->condensed convertible");
static_assert(!std::is_convertible<Nodal::CondensedMasterContribution,
    Nodal::CellMasterContribution>::value, "condensed->master convertible");

int main()
{
    Nodal::CellNodalData source = {};
    std::memset(&source, 0xAB, sizeof(source));

    Nodal::CellNodalData destination = {};
    std::memcpy(&destination, &source, sizeof(source));
    assert(std::memcmp(&destination, &source, sizeof(source)) == 0);

    Nodal::reset_storage(destination);
    Nodal::CellNodalData zero = {};
    assert(std::memcmp(&destination, &zero, sizeof(destination)) == 0);

    Nodal::StageStamp stamp = {};
    std::memset(&stamp, 0x11, sizeof(stamp));
    Nodal::reset_storage(stamp);
    Nodal::StageStamp zero_stamp = {};
    assert(std::memcmp(&stamp, &zero_stamp, sizeof(stamp)) == 0);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.t1b-layout-reset.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "layout_assertions": 12,
        "raw_assertions": 12,
        "distinct_assertions": 3,
        "roundtrip_fixture": True,
        "reset_fixture": True,
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="t1b-layout-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-T1B {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())