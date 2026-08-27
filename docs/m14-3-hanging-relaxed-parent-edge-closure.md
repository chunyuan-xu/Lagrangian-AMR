# M14.3 - Hanging Relaxed/Parent-Edge Package Closure

## Contract

`HydroCallbacks::quadrant_compute_relaxed_info_callback` and
`quadrant_parent_edge_matrix_callback` prepare parent-edge and relaxed info for
the hanging solve:

- relaxed info reads child/hanging geometry and writes owner-local relaxed
  fields;
- parent-edge matrix reads `ParentBounInfo` and writes owner-local `ideMcp` /
  `ideRHS`;
- ghost children are read-only inputs after the preceding exchange.

## Migration Boundary

The next step is to extract the parent-edge matrix algebra into a pure
`build_parent_edge_matrix_rhs(...)` helper. This package records the
input/output contract.

## Gate Closure

```text
package: M14.3
base commit: f41ec73
focused verification: hanging relaxed/parent-edge contract documented
G0: reused from M14.2 clean build PASS
G1: reused from M14.2 serial golden PASS
G3: reused from M14.2 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
