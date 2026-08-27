# M16.4 - Main Header-Dependency Cleanup Package Closure

## Contract

`main.cpp` includes inventoried and one proven unused/transitive include batch
removed. No functional include is removed without a compile/link check.

Removed from `src/main.cpp`:

- `alg.h`
- `amr/amr_criteria.h`
- `amr/amr_transfer.h`
- `amr/amr_controller.h`
- `amr/amr_callbacks.h`
- `physics/corner_solve.h`
- `solver/corner_solver.h`
- `solver/solver_gate.h`
- `solver/riemann_phases.h`
- `solver/hydro_phases.h`
- `hydro/hydro_callbacks.h`
- `hydro/hydro_controller.h`
- `solver/hydro_callbacks.h`
- `io/vtk_writer.h`
- `io/io_callbacks.h`
- `io/output_stamp.h`
- `physics/timestep_reduction.h`
- `physics/stage_policy.h`
- `diagnostics/state_invariant_checker.h`
- `mesh/ghost_session.h`
- `mesh/ghost_context.h`
- `mesh/cell_key.h`

Build/link verified and full gates rerun after the removal.

## Gate Closure

```text
package: M16.4
base commit: a963d56
focused verification: main header include batch removed
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
