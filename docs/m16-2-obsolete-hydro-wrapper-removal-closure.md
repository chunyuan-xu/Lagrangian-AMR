# M16.2 - Obsolete Hydro Wrapper Group Removal Closure

## Evidence

Hydro wrapper groups that become unused after M14 decomposition will be
identified by zero-caller probes before deletion.

## Action

Zero-caller audit completed for `src/hydro/hydro_callbacks.h`,
`src/solver/hydro_callbacks.h`, `src/hydro/hydro_controller.h`,
`src/solver/hydro_phases.h`, and `src/solver/riemann_phases.h`: every defined
`void` function has at least one declaration/call site in the repository. No
hydro wrapper group qualifies for deletion at this time.

## Gate Closure

```text
package: M16.2
base commit: 342eda5
focused verification: hydro wrapper zero-caller audit passed
G0: reused from M16.1 clean build PASS
G1: reused from M16.1 serial golden PASS
G3: reused from M16.1 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
