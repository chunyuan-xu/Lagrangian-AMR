# M13.5 - Coarsen Decision Package Closure

## Change

Added `src/amr/coarsen_decision_policy.h` with:

- `AMRCallbacks::coarsen_indicator_id`
- `AMRCallbacks::coarsen_indicator_mode`

Migrated `AMRAgorithm::CoarsenErrorEstimate` indicator/mode selection to use
these helpers.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m13_5_coarsen_decision_policy.py --summary .tmp/mg-m13-5-coarsen-decision-policy.json
MG-M13-5 PASS
```

## Gate Closure

```text
package: M13.5
base commit: 1daf040
focused verification: MG-M13-5 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
