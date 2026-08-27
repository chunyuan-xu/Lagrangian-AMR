# M14.6 - Coordinate Update Package Closure

## Contract

`HydroCallbacks::quadrant_update_corner_coordinate_callback` advances corner
coordinates and centroid coordinates from current to lag based on half-time
velocity and `dt_iter`.

## Implementation

Added `src/hydro/coordinate_kernel.h` with
`HydroCallbacks::update_corner_coordinates(...)` and migrated
`quadrant_update_corner_coordinate_callback`.

Added `python/test_m14_6_coordinate_kernel.py` to fixture the kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_6_coordinate_kernel.py --summary .tmp/mg-m14-6-coordinate-kernel.json
MG-M14-6 PASS
```

## Gate Closure

```text
package: M14.6
base commit: 6635a6f
focused verification: MG-M14-6 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
