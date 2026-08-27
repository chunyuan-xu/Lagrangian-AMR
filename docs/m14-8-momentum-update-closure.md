# M14.8 - Momentum Update Package Closure

## Contract

`HydroPhases::quadrant_update_momentum_callback` accumulates corner and
parent-edge forces plus relaxed fluxes, then advances `idCentroidVelo_lag` from
`idCentroidVelo_half` using `dt_iter` and `idMass`.

## Migration Boundary

Implemented: a pure `HydroCallbacks::update_momentum(CVariable&,
AMRCallbacks::ParentEdgeView&, int, int, double)` helper now owns the
momentum update. `HydroPhases::quadrant_update_momentum_callback` delegates to
it; no numerical formula changed.

Added:

- `src/hydro/momentum_kernel.h` — pure helper.
- `python/test_m14_8_momentum_kernel.py` — focused micro-gate.

## Gate Closure

```text
package: M14.8
base commit: 6a26f96
focused verification: momentum update kernel implemented and migrated
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
