# M13.4 - Refine Decision Package Closure

## Change

Added `src/amr/refine_decision_policy.h` with:

- `AMRCallbacks::refine_gradient_indicator_id`
- `AMRCallbacks::default_refine_tag_value`

Migrated:

- `AMRAgorithm::RefineErrorEstimate` indicator selection;
- `AMRCallbacks::quadrant_set_default_refining_tag_callback` concave-quad
  decision.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m13_4_refine_decision_policy.py --summary .tmp/mg-m13-4-refine-decision-policy.json
MG-M13-4 PASS
```

## Gate Closure

```text
package: M13.4
base commit: e6e85e8
focused verification: MG-M13-4 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
