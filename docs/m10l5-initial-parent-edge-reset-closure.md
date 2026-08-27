# M10L.5 - Initial-Leaf Reset Boundary Closure

## Change

Wired the M10L.4 scratch reset into `Initializer::Lagrangian_init_condition`,
so `FluxRelaxed` is defined for every `ParentBounInfo` slot at initial leaf
creation.

Added `AMRCallbacks::reset_initial_parent_edge_scratch(quad_data_t&)` in
`src/amr/parent_edge_scratch.h` to loop all four edges and apply the single-slot
reset. It preserves active-mask and all other fields; it only clears
`FluxRelaxed`.

## Preservation

The initial values set by `Lagrangian_init_condition` are untouched. The helper
does not change `IsParentChildBoun`, `Lcp`, `Ncp`, `Hanging_velocity`, or any
other parent-edge fields, and it does not change topology rebuild or exchange
order.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l5_initial_parent_edge_reset.py --summary .tmp/mg-m10l5-initial-parent-edge-reset.json
MG-M10L5 PASS
```

## Gate Closure

```text
package: M10L.5
base commit: 2f8a7a7
focused verification: MG-M10L5 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
