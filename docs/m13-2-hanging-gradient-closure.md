# M13.2 - Hanging-Gradient Package Closure

## Change

Added `AMRCallbacks::hanging_gradient` in
`src/amr/gradient_kernels.h`, a pure coarse/fine hanging gradient kernel:

```cpp
std::fabs(parent_para - child_para) / guarded_point_distance(parent_center, child_center);
```

Migrated the hanging branch in
`AMRCallbacks::quadrant_edge_minmod_estimate_callback` to use it for both fine
children, and removed the now-unused `dist1`/`dist2` local variables.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m13_2_hanging_gradient.py --summary .tmp/mg-m13-2-hanging-gradient.json
MG-M13-2 PASS
```

## Gate Closure

```text
package: M13.2
base commit: 4c2a81f
focused verification: MG-M13-2 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
