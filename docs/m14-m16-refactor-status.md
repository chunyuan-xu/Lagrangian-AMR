# M14-M16 Refactor Status (2026-08-28)

Status key: `implemented` = code changed and gate evidence recorded;
`documented` = contract/evidence recorded without new code in this package;
`dormant` = not active until a stated condition triggers.

## M14 Hydro conservative-update kernels

| Package | Status | Focused verification |
|---|---|---|
| M14.0 | documented | hydro phase contract |
| M14.1 | implemented | corner matrix/RHS kernel |
| M14.2 | implemented | regular-corner velocity kernel |
| M14.3 | implemented | parent-edge matrix/RHS kernel |
| M14.4 | implemented | hanging aggregate kernel |
| M14.5 | implemented | divergence kernel |
| M14.6 | implemented | coordinate kernel |
| M14.7 | implemented | volume/density kernel |
| M14.8 | implemented | momentum kernel |
| M14.9 | implemented | corner-work kernel |
| M14.10 | implemented | total/internal energy kernel |
| M14.11 | implemented | EOS / sound-speed kernels |
| M14.12 | implemented | named controller phase API |

M14.7-M14.12 all passed G0/G1/G3 and were pushed to `origin/main` as
`6a26f96`..`a963d56`.

## M15 Performance and measurement packages

| Package | Status | Focused verification |
|---|---|---|
| M15.0 | documented | profiling infrastructure contract |
| M15.1 | documented | frozen performance baseline accepted |
| M15.2x | dormant | no measured traversal-fusion candidate |
| M15.3x | dormant | no measured exchange-optimization candidate |
| M15.3P | dormant | activation condition not met |
| M15.4 | documented | no I/O producer measured as bottleneck |
| M15.5 | documented | memory/scaling report |

## M16 Cleanup packages

| Package | Status | Focused verification |
|---|---|---|
| M16.0 | implemented | obsolete diagnostics verified absent |
| M16.1 | audited | AMR wrapper zero-caller audit passed |
| M16.2 | audited | hydro wrapper zero-caller audit passed |
| M16.3x | dormant | no cold-field group selected for deletion |
| M16.4 | implemented | main header include batch removed |
| M16.5 | implemented | public headers self-contained |
| M16.6x | dormant | no translation-unit group selected |
| M16.7 | documented | final contracts and handoff |

M16.4 (`101c52f`) is pushed. M16.5 (`013beff`) and M16.0 (`4be36ef`) are
committed locally and pending push while GitHub connectivity is unstable.

## Out of current scope

- DBGF milestones (A/D/E/C) are frozen per user instruction.
- `src/nodal/**`, `reference/**`, tolerances, and golden fields were not
  modified.

## Final freeze / handoff

For the legacy optimization refactor scope (M10L-M16), the current branch is
the accepted handoff state:

- Implemented and gated: M10L.0-M10L.11, M11.0-M11.5, M12.0-M12.3,
  M13.0-M13.9, M14.0-M14.12, M16.0, M16.4, M16.5.
- Dormant until activation: M15.2x, M15.3x, M15.3P, M16.3x, M16.6x.
- Frozen per user instruction: DBGF milestones A0/A1/A2/A3, E1d-E6, D1-D4,
  C0. These remain documented but are not part of the current optimization
  refactor continuation.

No reference, tolerance, or golden-field changes were made in this branch.
