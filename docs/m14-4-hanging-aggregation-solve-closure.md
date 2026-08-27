# M14.4 - Hanging Aggregation/Solve Package Closure

## Contract

The hanging aggregation/solve phase includes:

- `quadrant_hanging_point_matrix_assemble_callback` aggregates fine and parent
  matrix/RHS contributions into owner-local `points[].MatrixP/RHS`;
- `quadrant_relaxed_hanging_solver_callback` solves the constrained hanging
  velocity and writes owner-local velocity, `idcnFluxRelaxed`, and parent-edge
  relaxed corrections;
- ghost records are read-only inputs after the preceding exchange.

## Migration Boundary

The next step is to extract the aggregation and constrained-solve algebra into
pure helpers. This package records the ownership contract.

## Gate Closure

```text
package: M14.4
base commit: 41c8fcc
focused verification: hanging aggregation/solve contract documented
G0: reused from M14.3 clean build PASS
G1: reused from M14.3 serial golden PASS
G3: reused from M14.3 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
