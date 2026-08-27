# M12.0 - Diagnostic Inventory and Options Package Closure

## Inventory

Startup diagnostic flags are now read once into the immutable
`Diagnostics::DiagnosticOptions` singleton:

| Env flag | Consumer | Extra work when enabled |
|---|---|---|
| `LAGRANGIAN_TRACE_TARGET` | `trace_target_snapshot`, `open_corner2_trace` | extra volume traversal, trace file writes |
| `LAGRANGIAN_TRACE_REFINE` | `refine_trace_enabled` | refine trace file writes |
| `LAGRANGIAN_VERBOSE_AMR` | `AMR_DEBUG_LOG` | extra production logs |
| `LAGRANGIAN_TRACE_CHECKSUM` | checksum trace | extra checksum traversal/collective |
| `LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE` | refresh snapshot | extra snapshot allocation and byte copies |
| `LAGRANGIAN_CHECK_STATE_INVARIANTS` | state invariant checker | extra invariant traversal/collective |
| `LAGRANGIAN_MEMORY_HIGH_WATER` | memory probe observer | extra exchange observation |

## Immutable Startup Options

Added `src/diagnostics/diagnostic_options.h`, which snapshots all flags once at
first access and exposes const getters. `core/trace.h` now reads from the
options singleton instead of calling `getenv` per check.

## Disabled-Mode Proof

The micro-gate runs in a child process with all diagnostic env flags removed
and asserts:

- every `DiagnosticOptions` getter is `false`;
- `target_trace_enabled()` and `refine_trace_enabled()` are `false`;
- `trace_riemann_iter()` default is `-1`.

This proves disabled mode performs no diagnostic traversal, output, or
collective through the trace gate functions.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m12_0_diagnostic_options.py --summary .tmp/mg-m12-0-diagnostic-options.json
MG-M12-0 PASS
```

## Gate Closure

```text
package: M12.0
base commit: b8eec7c
focused verification: MG-M12-0 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
