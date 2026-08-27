# M10L.8 - Balance Reset Boundary Closure

## Change

Added `AMRCallbacks::reset_balance_parent_edge_scratch(quad_data_t&)` and called
it from `quadrant_reset_hanging_info_callback`, which runs in
`refresh_after_balance` after balance replacement. It clears `FluxRelaxed` for
all four parent-edge slots and preserves all other fields.

## Ordering

The call is inserted before the existing hanging-flag resets. Existing rebuild,
ghost reconstruction, and publication order are unchanged.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l8_balance_parent_edge_reset.py --summary .tmp/mg-m10l8-balance-parent-edge-reset.json
MG-M10L8 PASS
```

## Gate Closure

```text
package: M10L.8
base commit: 699b409
focused verification: MG-M10L8 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
