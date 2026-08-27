# M14.7 - Volume/Density Update Package Closure

## Contract

`HydroPhases::quadrant_update_density_callback` recomputes `idVolume` from lag
corner coordinates and `idDensity_lag = idMass / idVolume`.

## Migration Boundary

The next step is to extract a pure `update_volume_density(...)` helper and
migrate the callback.

## Gate Closure

```text
package: M14.7
base commit: 02226c9
focused verification: volume/density update contract documented
G0: reused from M14.6 clean build PASS
G1: reused from M14.6 serial golden PASS
G3: reused from M14.6 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
