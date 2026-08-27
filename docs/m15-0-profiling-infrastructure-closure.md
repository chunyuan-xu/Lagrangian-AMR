# M15.0 - Profiling Infrastructure Package Closure

## Contract

The future profiling infrastructure must be:

- default-off via an immutable startup option;
- rank-local capture with no per-step MPI work;
- one end-of-run reduction only;
- bounded output with a fixed filename and no unbounded accumulation;
- disabled mode performs no extra traversal, output, or collective.

Relevant counters include phase seconds, AMR cycles, and exchange counts.

## Migration Boundary

This package records the contract before `M15.1` freezes the baseline. The
implementation will reuse the immutable `DiagnosticOptions` pattern.

## Gate Closure

```text
package: M15.0
base commit: 92d1171
focused verification: profiling infrastructure contract documented
G0: reused from M14.12 clean build PASS
G1: reused from M14.12 serial golden PASS
G3: reused from M14.12 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
