# M11.2 - Refinement-Variable Selection Package Closure

## Change

Added `AMRCallbacks::refinement_variable_ids(int)` in
`src/amr/refinement_variable_selector.h`, returning a typed
`RefinementVariableIds` with:

- `source_cell`
- `gradient_cell`
- `edge`
- `corner`

Unsupported criteria (`Distance` and unknown values) call `std::abort` before
any variable-array access.

Migrated the four gradient-estimation callbacks and removed their duplicate
switches:

- `AMRCallbacks::quadrant_edge_minmod_estimate_callback`
- `AMRCallbacks::quadrant_cell_minmod_estimate_callback`
- `HydroCallbacks::quadrant_set_gradient_zero_estimate_callback`
- `HydroCallbacks::quadrant_corner_minmod_estimate_callback`

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m11_2_refinement_variable_selector.py --summary .tmp/mg-m11-2-refinement-variable-selector.json
MG-M11-2 PASS
```

## Gate Closure

```text
package: M11.2
base commit: e49b2a9
focused verification: MG-M11-2 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
