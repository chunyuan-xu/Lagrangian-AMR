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

## Migration Boundary

The next step is to extract a pure `solve_regular_corner_velocity(...)` helper
and migrate the callback. This package records the input/output contract.

## Gate Closure

```text
package: M14.2
base commit: 226ea02
focused verification: regular-corner velocity contract documented
G0: reused from M14.1 clean build PASS
G1: reused from M14.1 serial golden PASS
G3: reused from M14.1 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
