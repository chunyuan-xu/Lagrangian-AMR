# M14.2 - Regular-Corner Velocity Package Closure

## Contract

`HydroCallbacks::quadrant_corner_velocity_callback` computes owner-local corner
velocity for regular corners:

- inputs: `points[].MatrixP`, `points[].RHS`, `TwoBouns`, and half-edge
  boundary types;
- boundary cases are handled by `CornerSolve::boundary_node_velocity`;
- interior case uses `MatrixInverse(MatrixP) * RHS`;
- result is written to `points[].velo_lag` and copied to
  `idcnVelocity_lag`.

## Ownership

The solve is owner-local for the velocity write; remote corners are read-only
inputs after the preceding exchange.

## Implementation

Added `src/hydro/corner_velocity_kernel.h` with
`HydroCallbacks::solve_regular_corner_velocity(...)` and migrated
`quadrant_corner_velocity_callback` to use it for owner-local velocity writes.

Added `python/test_m14_2_corner_velocity_kernel.py` to fixture interior and
boundary solves.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_2_corner_velocity_kernel.py --summary .tmp/mg-m14-2-corner-velocity-kernel.json
MG-M14-2 PASS
```

## Gate Closure

```text
package: M14.2
base commit: 226ea02
focused verification: MG-M14-2 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
