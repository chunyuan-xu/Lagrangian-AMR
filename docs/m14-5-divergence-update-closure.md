# M14.5 - Divergence Update Package Closure

## Contract

`HydroPhases::quadrant_compute_divergence_callback` computes
`idDivergence` from lag coordinates and lag corner velocities via
`PhysicalAlg::CalculateDivergence`.

## Implementation

Added `src/hydro/divergence_kernel.h` with
`HydroCallbacks::compute_divergence(...)` and migrated
`HydroPhases::quadrant_compute_divergence_callback`.

Added `python/test_m14_5_divergence_kernel.py` to fixture the kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_5_divergence_kernel.py --summary .tmp/mg-m14-5-divergence-kernel.json
MG-M14-5 PASS
```

## Gate Closure

```text
package: M14.5
base commit: 50c1bba
focused verification: MG-M14-5 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
