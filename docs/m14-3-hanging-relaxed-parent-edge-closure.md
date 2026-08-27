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

## Implementation

Added `src/hydro/parent_edge_matrix_kernel.h` with
`HydroCallbacks::build_parent_edge_matrix_rhs(...)` and migrated
`quadrant_parent_edge_matrix_callback` to use it.

Added `python/test_m14_3_parent_edge_matrix_kernel.py` to fixture the kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_3_parent_edge_matrix_kernel.py --summary .tmp/mg-m14-3-parent-edge-matrix-kernel.json
MG-M14-3 PASS
```

## Gate Closure

```text
package: M14.3
base commit: f41ec73
focused verification: MG-M14-3 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
