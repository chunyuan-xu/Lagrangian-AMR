# M14.7 - Volume/Density Update Package Closure

## Contract

`HydroPhases::quadrant_update_density_callback` recomputes `idVolume` from lag
corner coordinates and `idDensity_lag = idMass / idVolume`.

## Migration Boundary

Implemented: a pure `HydroCallbacks::update_volume_density(CVariable&, int)`
helper now owns the volume/density update. `HydroPhases::
quadrant_update_density_callback` delegates to it; no numerical formula
changed.

Added:

- `src/hydro/volume_density_kernel.h` — pure helper.
- `python/test_m14_7_volume_density_kernel.py` — focused micro-gate.

## Gate Closure

```text
package: M14.7
base commit: 454655e
focused verification: volume/density update kernel implemented and migrated
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
