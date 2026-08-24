import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

# Provides GeometryAlg::GetPolyCenter (declared in alg.h, defined in alg.cpp).
# The test only uses axis-aligned squares, where the centroid is the corner
# average — the same value the real implementation returns there.
SOURCE = r'''
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include <p4est.h>
#include <p4est_iterate.h>
#include "diagnostics/state_invariant_checker.h"

namespace GeometryAlg {
CDoubleVector GetPolyCenter(const CDoubleVector coord[4]) {
    return 0.25 * (coord[0] + coord[1] + coord[2] + coord[3]);
}
}

void CVariable::CVariableRisize() {}

static void set_valid_state(CVariable &v)
{
    // axis-aligned unit square
    const CDoubleVector c0(0.0, 0.0), c1(1.0, 0.0),
        c2(1.0, 1.0), c3(0.0, 1.0);
    v.corner_vector(idcnCoords_cur, 0) = c0;
    v.corner_vector(idcnCoords_cur, 1) = c1;
    v.corner_vector(idcnCoords_cur, 2) = c2;
    v.corner_vector(idcnCoords_cur, 3) = c3;
    for (int i = 0; i < CNDIM; ++i) {
        v.corner_vector(idcnCoords_lag, i) =
            v.corner_vector(idcnCoords_cur, i);
    }
    v.cell(idVolume) = 1.0;
    v.cell(idMass) = 1.0;
    v.cell(idDensity_cur) = 1.0;
    v.cell(idDensity_lag) = 1.0;
    v.cell(idInternalEnergy_cur) = 1.0;
    v.cell(idInternalEnergy_lag) = 1.0;
    v.cell(idGamma) = 1.4;
    v.cell(idPressure_cur) = 0.4;              // EOS(1.4, 1, 1)
    v.cell(idPressure_lag) = 0.4;
    v.cell(idSoundSpeed) = std::sqrt(1.4 * 0.4 / 1.0);
    v.cell(idTotalEnergy_cur) = 2.0;           // internal 1.0 + kinetic 1.0
    v.cell_vector(idCentroidVelo_cur) = CDoubleVector(1.0, 1.0);
    v.cell_vector(idCentroidCoord_cur) = CDoubleVector(0.5, 0.5);
}

static bool has_violation(const std::vector<Diagnostics::InvariantViolation> &v,
                          const char *name)
{
    for (size_t i = 0; i < v.size(); ++i) {
        if (std::string(v[i].name) == name) {
            return true;
        }
    }
    return false;
}

int main()
{
    // 1. valid state -> no violations
    {
        CVariable v;
        set_valid_state(v);
        assert(Diagnostics::check_cell_invariants(v).empty());
    }

    // 2. volume <= 0
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idVolume) = -1.0;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "volume>0"));
    }

    // 3. mass <= 0
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idMass) = -5.0;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "mass>0"));
    }

    // 4. density != mass/volume
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idDensity_cur) = 2.5;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "density=mass/volume"));
    }

    // 5. pressure != EOS
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idPressure_cur) = 0.4 + 1e-3;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "pressure=EOS"));
    }

    // 6. sound speed != c(gamma, p, rho)
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idSoundSpeed) = 0.0;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "sound_speed=c"));
    }

    // 7. total energy <= 0
    {
        CVariable v;
        set_valid_state(v);
        v.cell(idTotalEnergy_cur) = -1.0;
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "total_energy>0"));
    }

    // 8. stored centroid drifts from the geometry centroid
    {
        CVariable v;
        set_valid_state(v);
        v.cell_vector(idCentroidCoord_cur) = CDoubleVector(0.9, 0.9);
        assert(has_violation(Diagnostics::check_cell_invariants(v),
                             "centroid=geom"));
    }

    return 0;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="lagrangian_invariant_test_") as directory:
        source = Path(directory) / "test.cpp"
        executable = Path(directory) / "test.exe"
        source.write_text(SOURCE, encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join([
            "C:/msys64/usr/bin",
            "C:/msys64/ucrt64/bin",
            environment.get("PATH", ""),
        ])
        compile_result = subprocess.run(
            [
                str(CXX),
                "-std=c++14",
                "-Wall",
                "-Wextra",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return 1
        run_result = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        if run_result.returncode != 0:
            return run_result.returncode

    print("PASS: state invariant checker detects injected violations with CellKey")
    return 0


if __name__ == "__main__":
    sys.exit(main())
