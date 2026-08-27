# M10L.10 - Demand-Driven Typed View Package Closure

## Change

Added `AMRCallbacks::ParentEdgeView` in `src/amr/parent_edge_view.h`. It is an
ABI-neutral, non-owning view over `quad_data_t::m_pc_edge_data` with:

- `ParentBounInfo &at(int edge)` for mutable views;
- `const ParentBounInfo &at(int edge) const` for const views;
- `int size() const`.

Migrated `HydroPhases::quadrant_update_momentum_callback` from the raw
`ParentBounInfo *PCInfo = (ParentBounInfo *)&data->m_pc_edge_data;` cast to the
typed `ParentEdgeView`.

## Proofs

- Address parity: the view returns references whose addresses match
  `&data.m_pc_edge_data[edge]`.
- Const-write rejection: a `const ParentEdgeView::at` returns
  `const ParentBounInfo&`, which is not assignable.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l10_parent_edge_view.py --summary .tmp/mg-m10l10-parent-edge-view.json
MG-M10L10 PASS
```

## Gate Closure

```text
package: M10L.10
base commit: 479d38f
focused verification: MG-M10L10 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
