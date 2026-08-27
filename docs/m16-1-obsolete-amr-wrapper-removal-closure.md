# M16.1 - Obsolete AMR Wrapper Group Removal Closure

## Evidence

AMR wrapper groups that become unused after M13 decomposition will be
identified by zero-caller probes before deletion.

## Action

Zero-caller audit completed for `src/amr/amr_callbacks.h`: every defined
`void` function has at least one declaration/call site in the repository. No
AMR wrapper group qualifies for deletion at this time.

## Gate Closure

```text
package: M16.1
base commit: 342eda5
focused verification: AMR wrapper zero-caller audit passed
G0: reused from M16.0 clean build PASS
G1: reused from M16.0 serial golden PASS
G3: reused from M16.0 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
