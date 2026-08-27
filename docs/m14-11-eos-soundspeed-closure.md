# M14.11 - EOS and Sound-Speed Package Closure

## Contract

`HydroPhases::quadrant_update_EOS_callback` recomputes `idPressure_lag` from
gamma/density/internal energy. `quadrant_compute_soundspeed_callback` recomputes
`idSoundSpeed` from gamma/pressure/density.

## Migration Boundary

Implemented: pure `HydroCallbacks::update_eos(CVariable&)` and
`HydroCallbacks::update_sound_speed(CVariable&)` helpers now own the EOS and
sound-speed updates. `HydroPhases::quadrant_update_EOS_callback` and
`quadrant_compute_soundspeed_callback` delegate to them; no numerical formula
changed.

Added:

- `src/hydro/eos_kernel.h` — pure helpers.
- `python/test_m14_11_eos_kernel.py` — focused micro-gate.

## Gate Closure

```text
package: M14.11
base commit: 1bfb11b
focused verification: EOS and sound-speed kernels implemented and migrated
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
