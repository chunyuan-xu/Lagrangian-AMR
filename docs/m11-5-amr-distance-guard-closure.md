# M11.5 - AMR Distance-Guard Package Closure

## Change

Added a pure zero-distance guard `GeometryAlg::guarded_point_distance` in
`src/alg.h`. It returns the distance for valid geometry and aborts with a
label when the distance is not greater than `m_eps`. Invalid geometry is
rejected, never clamped.

Migrated the conforming, hanging, and corner gradient uses:

- AMR hanging gradient in `AMRCallbacks::quadrant_edge_minmod_estimate_callback`;
- AMR conforming gradient in the regular-face branch of the same callback;
- AMR corner gradient in
  `HydroCallbacks::quadrant_corner_minmod_estimate_callback`.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m11_5_zero_distance_guard.py --summary .tmp/mg-m11-5-zero-distance-guard.json
MG-M11-5 PASS
```

## Gate Closure

```text
package: M11.5
base commit: 510e17e
focused verification: MG-M11-5 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
