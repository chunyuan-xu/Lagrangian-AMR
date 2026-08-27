# M13.0 - AMR Authority Contract

## Rules

- A local quadrant is the only writable owner of its `quad_data_t` payload.
- Ghost records are read-only snapshots; writing through a ghost pointer is
  forbidden.
- Scratch writes are owner-local only.
- AMR transfer (`Lagrangian_replace_quads`) writes owner-local records during
  refine/coarsen.
- `GhostSession` must be invalidated after refine/coarsen/balance/partition
  and rebuilt before any remote read.

## Callback Inventory

| Callback | Reads | Writes | Exchange | Invalidates |
|---|---|---|---|---|
| `quadrant_edge_minmod_estimate_callback` | parent/child/brother `CVariable` | owner-local edge gradient fields | prior ghost exchange | no |
| `quadrant_cell_minmod_estimate_callback` | owner `CVariable` | owner-local cell gradient fields | no | no |
| `quadrant_whether_allowing_coarsening_from_edge_callback` | child/parent levels | owner-local `idAllowCoarsening` | prior exchange | no |
| `quadrant_whether_allowing_coarsening_from_corner_callback` | neighbor levels | owner-local `idAllowCoarsening` | prior exchange | no |
| `quadrant_update_after_balance_callback` | master/parent geometry and velocity | owner child geometry/velocity/volume/density/pressure | prior exchange | no |
| `quadrant_set_init_parent_edge_callback` | child/ghost points and parent geometry | owner parent `m_pc_edge_data` and parent `cndata` | prior exchange | no |
| `quadrant_get_children_hanging_info_callback` | child/ghost `hdata` | owner child `points[].IsHanging/TwoBouns` | prior exchange | no |
| `quadrant_reset_parent_edge_callback` | none | owner `m_pc_edge_data` + `pi_constrained_parent` | no | parent-edge scratch |
| `quadrant_reset_hanging_info_callback` | none | owner `points` flags + `m_pc_edge_data.addDiss` + `FluxRelaxed` | no | balance scratch |
| `Lagrangian_replace_quads` | owner/outgoing/incoming payloads | owner local records | no | nodal/scratch after transfer |

## Stable Keys

- Logical leaf identity uses `{treeid, level, x, y}`.
- `quadid` is local indexing only and is not a stable global key.
- Hanging/parent edges are addressed by local face/corner indices inside the
  callback context.

## Gate Closure

```text
package: M13.0
base commit: 1e55992
focused verification: AMR authority contract documented
G0: reused from M12.3 clean build PASS
G1: reused from M12.3 serial golden PASS
G3: reused from M12.3 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
