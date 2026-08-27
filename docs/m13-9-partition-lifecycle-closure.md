# M13.9 - Partition Lifecycle Package Closure

## Contract

After `p4est_partition`:

1. full canonical `quad_data_t` payload is migrated by p4est;
2. `GhostSession::invalidate_after_topology_change` marks the old session stale;
3. caller destroys/rebuilds the session before remote reads;
4. subsequent exchange publishes the migrated owner/ghost records.

No partial ghost schema or compact buffer is introduced. Partition remains
full-payload.

## Current State

`AMRController::execute_partition` already follows this order. This package
records the lifecycle contract for later isolation.

## Gate Closure

```text
package: M13.9
base commit: 64237be
focused verification: partition lifecycle contract documented
G0: reused from M13.8 clean build PASS
G1: reused from M13.8 serial golden PASS
G3: reused from M13.8 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
