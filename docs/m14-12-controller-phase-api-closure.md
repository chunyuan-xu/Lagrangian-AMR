# M14.12 - Controller Phase API Package Closure

## Contract

`HydroController::advance_single_stage` names the legacy phase sequence:

```text
boundary -> half-time -> corner geometry -> nodal mirror/local master
  -> exchange -> AMR hanging/parent-edge -> exchange -> Riemann
  -> divergence -> coordinate -> density -> momentum -> work -> energy -> EOS -> sound-speed
```

Implemented: `HydroController::advance_single_stage` now routes the sequence
through named phase functions while preserving the legacy phase order:

```text
stage_phase_boundary
  -> stage_phase_half_corner
  -> stage_phase_nodal_assemble
  -> stage_phase_riemann
  -> stage_phase_conservative_updates
```

`stage_phase_conservative_updates` keeps divergence, coordinate, density,
momentum, work, energy, EOS, and sound-speed ordering with existing checksum
traces. No numerical phase order or formula changed.

Changed:

- `src/hydro/hydro_controller.h` — named phase helpers + `advance_single_stage`
  delegation.

## Gate Closure

```text
package: M14.12
base commit: 5fd7c53
focused verification: controller phase API implemented and routed
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
