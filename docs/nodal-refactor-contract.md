# Nodal Solver Refactor Contract

This document freezes the legacy ownership and phase boundaries before the
DBGF nodal solver migration.  It is a maintenance contract: a milestone may
change one listed field group or callback only after updating this document
and passing G0, G1, and G3.

## Scope and authority

Until the DBGF candidate is explicitly selected, the legacy data is the only
authoritative production state:

- `quad_data_t::m_cndata`, `points`, and `m_pc_edge_data` hold corner,
  hanging, and parent-edge state.
- `CVariable` holds the legacy cell, corner, and edge flux/velocity state.
- `ParentBounInfo::FluxRelaxed` and `idcnFluxRelaxed` remain legacy-only
  corrections.  A DBGF contribution must never be added to either field.

New nodal fields are shadow-only until a whole-pipeline `Legacy` versus
`DBGFExperimental` selector exists.  No callback may mix the two pipelines
within one time step.

## Legacy Riemann phase graph

`HydroController::RiemannSolver` owns the current sequence:

```text
reset FluxRelaxed
  -> corner local matrix assembly (volume)
  -> inner ghost exchange (MatrixAssemble)
  -> corner-to-point matrix assembly (corner)
  -> outer ghost exchange (RiemannPhases)
  -> master corner solve (corner)
  -> copy lag velocity to relaxed velocity (volume)
  -> outer ghost exchange (RiemannPhases)
  -> relaxed/parent preparation (volume)
  -> parent-edge matrix (volume)
  -> inner ghost exchange (hanging solver preparation)
  -> hanging matrix assembly (face)
  -> inner ghost exchange (assembled hanging state)
  -> relaxed hanging solve (face)
  -> outer ghost exchange (RiemannPhases)
  -> compute local corner/edge forces (volume)
  -> momentum and work consumers (volume)
```

The shorter `RiemannPhases::run_iteration` sequence is the enclosing legacy
iteration contract: `assemble -> exchange -> master -> exchange -> hanging
-> exchange`.  A DBGF face reduction and DBGF master corner traversal must be
separate `p4est_iterate` calls with an exchange between them.

Before this graph, `advance_single_stage` runs the producer chain required by
the legacy records:

```text
boundary classification
  -> half-time state
  -> Rcp/Lcp/Ncp and impedance geometry
  -> ghost exchange
  -> AMR hanging/parent-edge discovery
       children hanging writer -> exchange -> parent reset -> parent init
  -> ghost exchange
  -> RiemannSolver
```

The two exchange layers are intentional facts of the current implementation:
`MatrixAssemble` and the hanging preparation function exchange internally,
while `RiemannPhases::run_iteration` exchanges after each supplied phase.
Removing an apparently redundant exchange is a separate behavior change.

## Legacy producer and consumer inventory

| Field group | Producers and resets | Production readers/consumers |
|---|---|---|
| `CHalf_edge_data` / `CCorner_data` (`m_cndata`) | `quadrant_get_BYD_callback` writes boundary type/value; `quadrant_compute_RcpLcpNcp_callback` writes geometry and impedance; `quadrant_set_init_parent_edge_callback` corrects coarse hanging-edge `Lcp` | local corner matrix assembly; corner-to-point assembly; AMR children-hanging discovery; hanging-point assembly; VTU corner-pressure output |
| `CPoint_data_t` (`points`) | corner-to-point assembly writes `MatrixP/RHS/TwoBouns`; children-hanging discovery writes `IsHanging/TwoBouns`; hanging-point assembly writes `MatrixP/RHS`, boundary records, parent record, and coordinates; corner solve writes `velo_lag`; `quadrant_reset_hanging_info_callback` resets selected flags | corner solve reads `MatrixP/RHS/TwoBouns`; relaxed hanging solve reads `MatrixP/RHS`; parent-edge initialization reads `IsHanging`, constrained pressure, and `TwoBouns` |
| `ParentBounInfo` (`m_pc_edge_data`) | `quadrant_reset_parent_edge_callback` resets topology/geometry/flux fields; `quadrant_set_init_parent_edge_callback` writes topology, normals, lengths, and `ParentPIStar`; `quadrant_parent_edge_matrix_callback` computes `Zcp`; relaxed hanging solve writes `IsParentChildBoun/addDiss/Hanging_velocity/FluxRelaxed`; hanging reset clears `addDiss` | parent-edge matrix consumes topology, velocity, geometry, and its newly computed `Zcp`; corner-force recovery, momentum update, and work update consume later fields |
| `idcnFluxRelaxed` | `quadrant_flux_relaxed_reset_callback` resets; relaxed hanging solve writes child corrections | momentum and work callbacks |
| `CEdge_data` (`m_edata`) | `quadrant_get_BYD_callback` writes boundary type | no current production reader found |

The following legacy members are dormant or incompletely initialized in the
current production search and must not be treated as valid inputs during the
migration:

- `CPoint_data_t::PI_hanging`, `add_dissipation_child1`, and
  `add_dissipation_child2` have no production producer or consumer.
- `CPoint_data_t::AddDiss` and `add_dissipation_parent` are reset but have no
  production consumer.
- `CPoint_data_t::pi_constrained_parent` is read by
  `quadrant_set_init_parent_edge_callback` to produce `ParentPIStar`, but no
  production writer was found.  This is an unproduced read and must not be
  copied into DBGF semantics.
- `CPoint_data_t::BounParent`, `master_coord_relaxed`, and `hanging_coord` are
  written by hanging-point assembly but have no production reader.
- `CHalf_edge_data::is_hanging` is constructor-defaulted and otherwise has no
  production consumer; `which_face` has no production producer or consumer.
- `ParentBounInfo::ParentPIStar` receives the unproduced point value but has
  no later production read.
- `CEdge_data` is boundary-written but otherwise dormant.

Dormant or unproduced does not authorize deletion before the D-phase
zero-reader and ABI checks.

## Ownership and communication

- A local quadrant is the only writable owner of its `quad_data_t` payload.
- `GhostSession::remote` exposes a read-only remote snapshot contract.  Some
  legacy callbacks cast its result, or related ghost data, back to mutable
  pointers and rely on `!is_ghost` guards at each write.  The audited writes
  in corner/hanging assembly and AMR parent/hanging discovery are guarded,
  but this is not const-correctness proof.  Those callbacks must be made
  const-clean during migration; a ghost pointer is never an authorized write
  target.
- The ghost buffer exchanges `sizeof(quad_data_t)` bytes for every ghost
  leaf.  New persistent fields must have fixed size, no pointers, no virtual
  functions, and no process-local handles.
- A coarse owner is the sole writer for a hanging face's condensed endpoint
  contributions.  Every cell owner later recovers its own local force after
  the master velocity exchange.
- `LeafCellKey{treeid,level,x,y}` identifies a leaf, but a leaf-local corner
  tuple is not a canonical node identity across trees or refinement levels.
  Cross-leaf/master diagnostics use a pure-value `CanonicalNodeKey` derived
  through p4est connectivity and orientation canonicalization.  Hanging-face
  ledgers use a pure-value `CanonicalHangingFaceKey` with canonical endpoints
  and segment identity.  These are diagnostic/verification values, not global
  production objects.  `quadid` is local indexing only and must never identify
  an MPI-global cell, node, or face.

`LeafCellKey`, `CanonicalNodeKey`, and `CanonicalHangingFaceKey` are required
contracts, not current implementation claims.  Their connectivity- and
orientation-aware implementation is the T2 deliverable.  Current `quadid`
uses are ghost-buffer/local indices and local diagnostics only.

## Invalidation boundaries

The following events invalidate every transient nodal cache: geometry update,
Riemann iteration change, time-step/substage change, refine, coarsen, balance,
partition, and ghost-session rebuild.  A future `StageStamp` must encode the
topology generation, geometry epoch, time step, substage, Riemann iteration,
and phase.

Current lifecycle operations are narrower than that future rule:

| Boundary | Current operation | DBGF requirement |
|---|---|---|
| parent-edge discovery | `quadrant_reset_parent_edge_callback` before parent initialization | reset every parent-face shadow stage before write |
| post-balance refresh | `quadrant_reset_hanging_info_callback` and parent-edge reset/initialization paths | increment topology/geometry epoch and rebuild all affected shadow data |
| refine/coarsen/balance/partition | `GhostSession::invalidate_after_topology_change`, followed by destroy/rebuild in the controller/caller | reject stale `StageStamp` before every remote read |
| refresh idempotence diagnostic | `append_refresh_snapshot` copies raw local and ghost payload bytes | shadow resets must make every compared byte deterministic |

AMR uses raw payload transfer and refresh snapshots.  No migration may assert
that the existing whole `quad_data_t` is trivially copyable without a separate
whole-record lifecycle audit.  New storage itself must be scalar POD with
explicit reset functions.

## Migration prohibitions

- Do not delete or rename `ParentBounInfo` while the legacy relaxed solver or
  momentum/work callbacks still read it.
- Do not persist an aggregated hanging `M_h,b_h` as a cell physical flux.
  DBGF aggregation is face-local assembly state; evaluated cell force is a
  different field group.
- Do not activate cylindrical DBGF physics in this migration.  Only a pure
  segment-geometry test may exercise cylindrical endpoint weights.
- Do not update `reference/`, relax the comparison tolerance, or accept a
  rerun-only MPI pass as a milestone closure.

## Required evidence per code milestone

Each focused change records the changed callback/field group, authority
source, reset point, exchange before each remote read, and G0/G1/G3 summaries.
Any failure returns to the most recent passing focused commit; it is not
handled by a face-level or node-level legacy fallback.
