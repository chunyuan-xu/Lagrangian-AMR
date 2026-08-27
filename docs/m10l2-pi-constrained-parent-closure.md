# M10L.2 - Confirmed Read-Before-Write Repair Closure

## Hypothesis

`CPoint_data_t::pi_constrained_parent` was read by
`quadrant_set_init_parent_edge_callback` to produce
`ParentBounInfo::ParentPIStar`, but no production writer existed. The minimal
repair is to define the field before that read without changing numerical
behavior or broadening lifecycle/transfer contracts.

## Focused Change

- Added `src/amr/parent_edge_scratch.h` with the proven writer
  `AMRCallbacks::reset_point_pi_constrained_parent`, which zeroes every
  `points[].pi_constrained_parent`.
- Wired that helper into `quadrant_reset_parent_edge_callback`, which runs
  immediately before `quadrant_set_init_parent_edge_callback` in
  `Get_AMR_BDY_info`.
- Added `python/test_m10l2_pi_constrained_parent_reset.py` to poison the field
  and prove the writer overwrites it.

Storage is retained; reset/transfer behavior is not broadened to any new event.
`ParentPIStar` remains dormant in the current reader inventory.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l2_pi_constrained_parent_reset.py --summary .tmp/mg-m10l2-pi-constrained-parent-reset.json
MG-M10L2 PASS
```

## Gate Closure

```text
package: M10L.2
base commit: fc2bc0c
focused verification: MG-M10L2 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged (17 reference files, no diff)
closure commit: this package's single HEAD commit
```
