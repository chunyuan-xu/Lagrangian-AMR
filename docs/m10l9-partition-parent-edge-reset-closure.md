# M10L.9 - Partition Reset Boundary Closure

## Decision

No new partition reset call is required. Partition migrates the complete
canonical `quad_data_t` payload via `p4est_partition`; it does not create new
leaves and does not run a construction path. The `FluxRelaxed` transient field
is therefore preserved byte-for-byte from the source rank.

M10L.5-M10L.8 already define `FluxRelaxed` at initial creation, refine, coarsen,
and balance. A leaf entering partition has its parent-edge scratch already
defined, so partition simply carries that defined value to the destination
rank. Full-payload migration remains the contract; no compact ghost schema or
partial record is introduced.

## Evidence

- `AMRController::execute_partition` calls `p4est_partition(p4est,
  allowcoarsening, NULL)` with no replace callback that creates payload state.
- `GhostSession` is invalidated after partition; the next rebuild exchanges
  complete `quad_data_t` records.
- No production source change is made in this package.

## Gate Closure

```text
package: M10L.9
base commit: 420a141
focused verification: partition full-payload preservation proved by contract
G0: reused from M10L.8 clean build PASS
G1: reused from M10L.8 serial golden PASS
G3: reused from M10L.8 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
