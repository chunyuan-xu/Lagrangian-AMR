# M10L.7 - Coarsen-Parent Reset Boundary Closure

## Change

Added `AMRCallbacks::reset_coarsen_parent_parent_edge_scratch(quad_data_t&)` and
called it from the coarsen branch of `AMRCallbacks::Lagrangian_replace_quads`
for the coarsen-created parent, after the existing nodal reset. It clears
`FluxRelaxed` for all four parent-edge slots and preserves all other fields.

## Coverage

- Coarsen-created parents only: the call is inside the `num_outgoing > 1`
  branch.
- Preserves active-mask, `Lcp`, `Ncp`, `Hanging_velocity`, and topology order.
- Does not change exchange order or any transfer formula.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l7_coarsen_parent_parent_edge_reset.py --summary .tmp/mg-m10l7-coarsen-parent-parent-edge-reset.json
MG-M10L7 PASS
```

## Gate Closure

```text
package: M10L.7
base commit: 0579fa6
focused verification: MG-M10L7 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
