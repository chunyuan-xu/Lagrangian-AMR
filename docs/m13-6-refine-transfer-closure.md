# M13.6 - Refine-Transfer Package Closure

## Contract

During refine transfer:

- parent record is read-only for geometry/physical source data;
- each child record is an owner-local write target;
- `AMRTransfer::refine_distribute_buffers` distributes parent geometry and
  physical buffers to children;
- `HydroCallbacks::generate_children_info_from_parent` refreshes child info;
- nodal and parent-edge scratch are reset before transfer.

## Current State

The refine branch in `AMRCallbacks::Lagrangian_replace_quads` already routes to
the extracted buffer distributor. Remaining inline copy loops and conservation
checks stay in the callback for now; this package records the ownership
contract so the later copy-loop extraction can proceed without changing the
authority model.

## Gate Closure

```text
package: M13.6
base commit: 0c47df1
focused verification: refine transfer contract documented
G0: reused from M13.5 clean build PASS
G1: reused from M13.5 serial golden PASS
G3: reused from M13.5 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
