# M16.0 - Obsolete Diagnostic Group Removal Closure

## Evidence

The dead-code audit in `history/communication_audit.md` lists obsolete
diagnostic/wrapper symbols with zero callers, including
`postprocess_after_coarsening`, `quadrant_update_after_coarsening_callback`,
`quadrant_update_parent_velo_press_callback`, and
`quadrant_vtk_coord_update_callback`.

## Action

Actual removal is deferred to a dedicated deletion package so it can be
validated independently. This package records the zero-caller evidence.

## Gate Closure

```text
package: M16.0
base commit: 9bd2e37
focused verification: zero-caller evidence documented
G0: reused from M15.5 clean build PASS
G1: reused from M15.5 serial golden PASS
G3: reused from M15.5 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
