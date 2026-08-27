# M10L.6 - Refine-Child Reset Boundary Closure

## Change

Added `AMRCallbacks::reset_refine_child_parent_edge_scratch(quad_data_t&)` and
called it from the refine branch of
`AMRCallbacks::Lagrangian_replace_quads` for every refine-created child, after
the existing nodal reset. It clears `FluxRelaxed` for all four parent-edge slots
and preserves all other fields.

## Coverage

- Refine-created children only: the call is inside the refine branch, not the
  coarsen branch.
- Preserves active-mask, `Lcp`, `Ncp`, `Hanging_velocity`, and topology order.
- Does not change exchange order or any existing transfer formula.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l6_refine_child_parent_edge_reset.py --summary .tmp/mg-m10l6-refine-child-parent-edge-reset.json
MG-M10L6 PASS
```

## Gate Closure

```text
package: M10L.6
base commit: b3bead8
focused verification: MG-M10L6 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
