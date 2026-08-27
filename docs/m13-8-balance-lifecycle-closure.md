# M13.8 - Balance Lifecycle Package Closure

## Contract

After `p4est_balance_ext`:

1. balance replace callback resets nodal/scratch state and writes owner-local
   records;
2. `GhostSession::invalidate_after_topology_change` marks the old session stale;
3. `refresh_after_balance` rebuilds the session and re-runs the balance refresh
   callbacks;
4. subsequent exchange publishes the rebuilt owner/ghost records.

Existing `reset_balance_parent_edge_scratch` is invoked in
`quadrant_reset_hanging_info_callback` during `refresh_after_balance`.

## Current State

The order in `AMRController::execute_amr` and `AMRCallbacks::refresh_after_balance`
is preserved. This package records the lifecycle contract for later isolation.

## Gate Closure

```text
package: M13.8
base commit: 48c5600
focused verification: balance lifecycle contract documented
G0: reused from M13.7 clean build PASS
G1: reused from M13.7 serial golden PASS
G3: reused from M13.7 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
