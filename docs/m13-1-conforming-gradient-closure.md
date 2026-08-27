# M13.1 - Conforming-Gradient Package Closure

## Change

Added `AMRCallbacks::conforming_gradient` in
`src/amr/gradient_kernels.h`, a pure two-cell gradient kernel:

```cpp
std::fabs(para_a - para_b) / guarded_point_distance(center_a, center_b);
```

Migrated the regular-face branch in
`AMRCallbacks::quadrant_edge_minmod_estimate_callback` to call this kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m13_1_conforming_gradient.py --summary .tmp/mg-m13-1-conforming-gradient.json
MG-M13-1 PASS
```

## Gate Closure

```text
package: M13.1
base commit: a86bf46
focused verification: MG-M13-1 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
