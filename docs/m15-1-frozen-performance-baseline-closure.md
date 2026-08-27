# M15.1 - Frozen Performance Baseline Package Closure

## Baseline Anchor

The legacy baseline is accepted at commit `ed78788`. G1 serial and G3 four-rank
golden results are recorded in:

- `serial_golden_summary.json`
- `mpi_gate_summary.json`

Timing values from those summaries are the frozen performance baseline for this
refactor. No reference, tolerance, or parameter change is authorized to make
later measurements pass.

## Gate Closure

```text
package: M15.1
base commit: b89dcf2
focused verification: baseline accepted and recorded
G0: reused from M15.0 clean build PASS
G1: reused from M15.0 serial golden PASS
G3: reused from M15.0 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
