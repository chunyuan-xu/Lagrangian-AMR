# M12.1 - Riemann Targeted-Trace Package Closure

## Change

Moved the step-3 Riemann corner-velocity dump out of
`HydroController::advance_single_stage` into
`Diagnostics::dump_riemann_target_if_enabled` in
`src/diagnostics/riemann_target_trace.h`.

Changes:

- stable logical-cell matching via `is_trace_fine` (no `quadid == 397`
  dependency);
- centralized bounded filename construction via
  `Diagnostics::riemann_trace_filename`;
- disabled mode returns before any `p4est_iterate`, file open, or output.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m12_1_riemann_target_trace.py --summary .tmp/mg-m12-1-riemann-target-trace.json
MG-M12-1 PASS
```

## Gate Closure

```text
package: M12.1
base commit: fd0818f
focused verification: MG-M12-1 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
