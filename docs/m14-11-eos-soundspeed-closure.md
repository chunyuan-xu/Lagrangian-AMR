# M14.11 - EOS and Sound-Speed Package Closure

## Contract

`HydroPhases::quadrant_update_EOS_callback` recomputes `idPressure_lag` from
gamma/density/internal energy. `quadrant_compute_soundspeed_callback` recomputes
`idSoundSpeed` from gamma/pressure/density.

## Migration Boundary

The next step is to extract pure `update_eos(...)` and `update_sound_speed(...)`
helpers and migrate the callbacks.

## Gate Closure

```text
package: M14.11
base commit: 4ecd253
focused verification: EOS/sound-speed contract documented
G0: reused from M14.10 clean build PASS
G1: reused from M14.10 serial golden PASS
G3: reused from M14.10 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
