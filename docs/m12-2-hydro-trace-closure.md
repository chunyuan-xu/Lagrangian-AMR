# M12.2 - Hydro Edge and Hanging-Trace Package Closure

## Change

Added `Diagnostics::trace_parent_edge_matrix` in
`src/diagnostics/hydro_trace.h`. The raw `edge_matrix_dbg_*` trace block inside
`HydroCallbacks::quadrant_parent_edge_matrix_callback` was removed and replaced
with a read-only diagnostic record call.

The remaining hydro trace sites already go through `open_corner2_trace` /
`target_trace_enabled`; this package removes the last raw `sprintf`/`fopen`
diagnostic ownership from a numerical callback.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m12_2_hydro_trace.py --summary .tmp/mg-m12-2-hydro-trace.json
MG-M12-2 PASS
```

## Gate Closure

```text
package: M12.2
base commit: 679de0a
focused verification: MG-M12-2 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
