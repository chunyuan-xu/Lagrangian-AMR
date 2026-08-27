# M14.9 - Corner-Work Update Package Closure

## Contract

`HydroPhases::quadrant_compute_work_callback` computes `idKineticVariation`
and `idTotalWork` from corner/parent forces, relaxed fluxes, velocities, and
geometry factors.

## Migration Boundary

Implemented: a pure `HydroCallbacks::update_work(CVariable&,
ParentBounInfo const*, int)` helper now owns the corner-work update.
`HydroPhases::quadrant_compute_work_callback` delegates to it; no numerical
formula changed.

Added:

- `src/hydro/work_kernel.h` — pure helper.
- `python/test_m14_9_work_kernel.py` — focused micro-gate.

## Gate Closure

```text
package: M14.9
base commit: 5094a5b
focused verification: corner-work update kernel implemented and migrated
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
