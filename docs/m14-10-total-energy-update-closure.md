# M14.10 - Total-Energy Update Package Closure

## Contract

`HydroPhases::quadrant_update_energy_callback` advances `idTotalEnergy_lag` and
`idInternalEnergy_lag` from half fields using `dt_iter`, `idTotalWork`, and
`idKineticVariation`, with Taylor-Green source handling.

## Migration Boundary

Implemented: a pure `HydroCallbacks::update_energy(CVariable&, double, int,
int)` helper now owns the total/internal energy update, including Taylor-Green
source and positivity guard. `HydroPhases::quadrant_update_energy_callback`
delegates to it; no numerical formula changed.

Added:

- `src/hydro/energy_kernel.h` — pure helper.
- `python/test_m14_10_energy_kernel.py` — focused micro-gate.

## Gate Closure

```text
package: M14.10
base commit: 32d54a9
focused verification: total/internal energy kernel implemented and migrated
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
