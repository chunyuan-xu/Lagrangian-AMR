# M14.0 - Hydro Phase Contract

## Phase Graph

`HydroController::advance_single_stage` owns the legacy phase order:

```text
boundary classification
  -> half-time state
  -> corner geometry/impedance (Rcp/Lcp/Ncp/Zcp)
  -> nodal boundary/geometry mirror and local master
  -> exchange
  -> AMR hanging/parent-edge discovery
  -> exchange
  -> RiemannSolver
  -> divergence/coordinate/density/momentum/work/energy/EOS/sound-speed
```

## Callback Read/Write Summary

| Callback | Reads | Writes | Exchange before first remote read |
|---|---|---|---|
| `quadrant_compute_halftime_variable_callback` | current/lag fields | half fields | no |
| `quadrant_compute_RcpLcpNcp_callback` | reconstruct fields | `m_cndata` geometry/impedance | no |
| `quadrant_corner_matrix_assemble_callback` | cell/corner fields + `m_cndata` | `MarCnData[idcnMcp]`, `idcnRHS` | yes (MatrixAssemble) |
| `quadrant_corner_to_point_matrix_assemble_callback` | remote corner matrices | owner/ghost `points[].MatrixP/RHS` | yes |
| `quadrant_corner_velocity_callback` | `points[].MatrixP/RHS/TwoBouns` | owner/ghost `velo_lag`, `idcnVelocity_lag` | yes |
| `quadrant_parent_edge_matrix_callback` | parent-edge info | `ideMcp`, `ideRHS` | no (owner local) |
| `quadrant_relaxed_hanging_solver_callback` | remote hanging state | owner/ghost hanging corrections | yes |
| `HydroPhases::quadrant_update_*_callback` | current/half/lag fields | lag fields | no |

## Current/Half/Lagged Transitions

- current fields are written by init, AMR transfer, coordinate update, and
  accept-solution;
- half fields are produced by `CalculateHalfTimeVariable`;
- lag fields are produced by momentum/energy/EOS updates and consumed by the
  next stage's half-time and output.

## Exchange-to-First-Reader Links

- `MatrixAssemble`: exchange before `quadrant_corner_to_point_matrix_assemble_callback`;
- `ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes`: exchanges
  before hanging point assembly and relaxed hanging solve;
- `ComputeCornerNodeVelocity`: exchange before `quadrant_corner_velocity_callback`;
- AMR cycle: invalidate/rebuild before remote AMR reads.

## Mixed Responsibilities

Some callbacks currently combine numerical writes with trace/output logic. The
M14 series will split those into pure kernels plus routing.

## Gate Closure

```text
package: M14.0
base commit: c2090fd
focused verification: hydro phase contract documented
G0: reused from M13.9 clean build PASS
G1: reused from M13.9 serial golden PASS
G3: reused from M13.9 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
