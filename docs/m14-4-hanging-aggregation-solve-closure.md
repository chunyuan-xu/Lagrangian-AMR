# M14.4 - Hanging Aggregation/Solve Package Closure

## Contract

The hanging aggregation/solve phase includes:

- `quadrant_hanging_point_matrix_assemble_callback` aggregates fine and parent
  matrix/RHS contributions into owner-local `points[].MatrixP/RHS`;
- `quadrant_relaxed_hanging_solver_callback` solves the constrained hanging
  velocity and writes owner-local velocity, `idcnFluxRelaxed`, and parent-edge
  relaxed corrections;
- ghost records are read-only inputs after the preceding exchange.

## Implementation

Added `src/hydro/hanging_aggregate_kernel.h` with
`HydroCallbacks::aggregate_hanging_matrix_rhs(...)` and migrated
`quadrant_hanging_point_matrix_assemble_callback` to use it.

Added `python/test_m14_4_hanging_aggregate_kernel.py` to fixture the kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_4_hanging_aggregate_kernel.py --summary .tmp/mg-m14-4-hanging-aggregate-kernel.json
MG-M14-4 PASS
```

## Gate Closure

```text
package: M14.4
base commit: 41c8fcc
focused verification: MG-M14-4 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
