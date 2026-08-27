# M12.3 - AMR Transfer-Trace Package Closure

## Change

Added `Diagnostics::RefineTraceFile` in
`src/diagnostics/amr_transfer_trace.h`. The raw `refine_dbg_*` trace block in
`AMRCallbacks::Lagrangian_replace_quads` was replaced with this RAII helper,
which:

- centralizes bounded, rank-aware filename construction;
- performs no file operation when refine tracing is disabled;
- writes the parent record at construction and child records via
  `write_child`.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m12_3_amr_transfer_trace.py --summary .tmp/mg-m12-3-amr-transfer-trace.json
MG-M12-3 PASS
```

## Gate Closure

```text
package: M12.3
base commit: a9731d9
focused verification: MG-M12-3 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
