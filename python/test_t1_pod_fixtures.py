"""T1a micro-gate: C++ compile helper, passing POD fixture, rejected fixture."""

import argparse
import json
import os
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")
DEFAULT_SUMMARY = ROOT / ".tmp" / "mg-t1a-pod-fixtures.json"

SOURCE = r"""
#include <cstdint>
#include <string>
#include <type_traits>

template <typename T>
struct RawByteEligible : std::integral_constant<bool,
    std::is_standard_layout<T>::value &&
    std::is_trivially_copyable<T>::value &&
    std::is_trivially_destructible<T>::value> {};

struct PositivePODFixture {
    std::uint64_t generation;
    std::uint32_t step;
    std::uint8_t phase;
    std::uint8_t role;
    double value[2];
};

struct NegativeFixture {
    std::string owner;   // non-trivial, pointer-bearing object
    double value;
};

static_assert(RawByteEligible<PositivePODFixture>::value,
    "positive POD fixture must be accepted");
static_assert(!RawByteEligible<NegativeFixture>::value,
    "negative fixture must be rejected");

int main() { return 0; }
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "lagrangian-amr.micro-gate.t1a-pod-fixtures.v1",
        "started_at": datetime.now().astimezone().isoformat(),
        "assertion_count": 2,
        "positive_fixture": "PositivePODFixture",
        "negative_fixture": "NegativeFixture",
        "status": "ERROR",
    }
    if not CXX.exists():
        summary["error"] = f"compiler not found: {CXX}"
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 2
    with tempfile.TemporaryDirectory(prefix="t1a-pod-", dir=ROOT / ".tmp") as directory:
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
    print(f"MG-T1A {summary['status']}: {args.summary}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())