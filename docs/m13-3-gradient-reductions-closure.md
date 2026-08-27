# M13.3 - Cell-and-Corner Gradient Reduction Package Closure

## Change

Added two pure max reductions in `src/amr/gradient_kernels.h`:

- `AMRCallbacks::reduce_max_edge_to_cell`
- `AMRCallbacks::reduce_max_corner_neighbor`

Migrated:

- `AMRCallbacks::quadrant_cell_minmod_estimate_callback` edge-to-cell reduction;
- `HydroCallbacks::quadrant_corner_minmod_estimate_callback` corner-neighbor
  reduction.

Exchange boundaries are unchanged.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m13_3_gradient_reductions.py --summary .tmp/mg-m13-3-gradient-reductions.json
MG-M13-3 PASS
```

## Gate Closure

```text
package: M13.3
base commit: 3e45d68
focused verification: MG-M13-3 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
