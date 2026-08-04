import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

SOURCE = r'''
#include <cassert>
#include <type_traits>
#include "variable.h"

CVariable::CVariable() {}
CVariable::~CVariable() {}
void CVariable::CVariableRisize() {}

int main()
{
    static_assert(std::is_same<
        decltype(static_cast<CVariable *>(nullptr)->cell(idMass)),
        double &>::value, "mutable cell accessor must return a reference");
    static_assert(std::is_same<
        decltype(static_cast<const CVariable *>(nullptr)->cell(idMass)),
        const double &>::value, "const cell accessor must return a const reference");

    CVariable value;
    const CVariable &constant = value;

    for (int id = 0; id < idDoubleCellVariableNum; ++id) {
        DoubleCellVariableID typed = static_cast<DoubleCellVariableID>(id);
        assert(&value.cell(typed) == &value.DouCData[id]);
        assert(&constant.cell(typed) == &value.DouCData[id]);
        value.cell(typed) = 1000.0 + id;
        assert(value.DouCData[id] == 1000.0 + id);
    }
    for (int id = 0; id < idDoubleCornerVariableNum; ++id) {
        for (int corner = 0; corner < CNDIM; ++corner) {
            DoubleCornerVariableID typed =
                static_cast<DoubleCornerVariableID>(id);
            assert(&value.corner(typed, corner) ==
                &value.DouCnData[id][corner]);
        }
    }
    for (int id = 0; id < idDoubleEdgeVariableNum; ++id) {
        for (int edge = 0; edge < CNDIM; ++edge) {
            DoubleEdgeVariableID typed = static_cast<DoubleEdgeVariableID>(id);
            assert(&value.edge(typed, edge) == &value.DouEData[id][edge]);
        }
    }
    for (int id = 0; id < idVectorCornerVariableNum; ++id) {
        for (int corner = 0; corner < CNDIM; ++corner) {
            VectorCornerVariableID typed =
                static_cast<VectorCornerVariableID>(id);
            assert(&value.corner_vector(typed, corner) ==
                &value.VecCnData[id][corner]);
        }
    }
    for (int id = 0; id < idVectorCellVariableNum; ++id) {
        VectorCellVariableID typed = static_cast<VectorCellVariableID>(id);
        assert(&value.cell_vector(typed) == &value.VecCData[id]);
        assert(&constant.cell_vector(typed) == &value.VecCData[id]);
        value.cell_vector(typed) = CDoubleVector(100.0 + id, 200.0 + id);
        assert(value.VecCData[id].x == 100.0 + id);
        assert(value.VecCData[id].y == 200.0 + id);
    }
    for (int id = 0; id < idVectorEdgeVariableNum; ++id) {
        for (int edge = 0; edge < CNDIM; ++edge) {
            VectorEdgeVariableID typed =
                static_cast<VectorEdgeVariableID>(id);
            assert(&value.edge_vector(typed, edge) ==
                &value.VecEdata[id][edge]);
        }
    }
    for (int id = 0; id < idIntCellVariableNum; ++id) {
        IntCellVariableID typed = static_cast<IntCellVariableID>(id);
        assert(&value.int_cell(typed) == &value.IntCData[id]);
        assert(&constant.int_cell(typed) == &value.IntCData[id]);
        value.int_cell(typed) = 7 + id;
        assert(value.IntCData[id] == 7 + id);
    }
    for (int id = 0; id < idIntEdgeVariableNum; ++id) {
        for (int edge = 0; edge < CNDIM; ++edge) {
            IntEdgeVariableID typed = static_cast<IntEdgeVariableID>(id);
            assert(&value.int_edge(typed, edge) == &value.IntEData[id][edge]);
        }
    }

    return 0;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="lagrangian_accessor_test_") as directory:
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

    print("PASS: typed accessors alias the legacy storage without layout changes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
