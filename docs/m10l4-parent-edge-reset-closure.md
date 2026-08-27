# M10L.4 - Parent-Edge Reset Contract Package Closure

## Contract

For one `ParentBounInfo` slot, `FluxRelaxed` is transient solver state:

- when `IsParentChildBoun == false`, consumers must not rely on `FluxRelaxed`;
- before any inactive read, the slot must be defined;
- resetting the slot clears only `FluxRelaxed` and leaves other bytes unchanged.

## Focused Change

Added `AMRCallbacks::reset_parent_edge_scratch(ParentBounInfo&)` in
`src/amr/parent_edge_scratch.h`. It resets `FluxRelaxed` to `(0,0)` and is
intentionally **not wired into any event** in this package.

Added `python/test_m10l4_parent_edge_reset.py` with:

- inactive-poison: `IsParentChildBoun == false`, poisoned `FluxRelaxed` is
  cleared by the reset;
- active-parity: `IsParentChildBoun == true`, known `FluxRelaxed` is also
  cleared;
- byte-scope: after reset only the `FluxRelaxed` bytes change.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l4_parent_edge_reset.py --summary .tmp/mg-m10l4-parent-edge-reset.json
MG-M10L4 PASS
```

## Gate Closure

```text
package: M10L.4
base commit: 530be40
focused verification: MG-M10L4 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
