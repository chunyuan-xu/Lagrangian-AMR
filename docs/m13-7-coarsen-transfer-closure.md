# M13.7 - Coarsen-Transfer Package Closure

## Contract

During coarsen transfer:

- each child record is read-only source data;
- the parent record is an owner-local write target;
- `AMRTransfer::coarsen_children_to_parent` aggregates geometry, physical
  buffers, mass, volume, density, energy, and EOS fields;
- nodal and parent-edge scratch are reset before transfer.

## Current State

The coarsen branch in `AMRCallbacks::Lagrangian_replace_quads` already routes to
the extracted `coarsen_children_to_parent`. Remaining inline validation and
conservation checks stay in the callback; this package records the ownership
contract for the later callback-slimming work.

## Gate Closure

```text
package: M13.7
base commit: 9facf62
focused verification: coarsen transfer contract documented
G0: reused from M13.6 clean build PASS
G1: reused from M13.6 serial golden PASS
G3: reused from M13.6 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
