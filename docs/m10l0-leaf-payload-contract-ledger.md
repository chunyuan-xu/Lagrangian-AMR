# M10L.0 - Leaf Payload Contract Ledger

Status: draft for closure
Package: M10L.0
Scope: audit only, no production source changes
Base commit: `ed78788` (accepted implementation anchor)

## 1. Purpose

This ledger records the current authoritative leaf payload contract for the
legacy solver before later refactoring changes the ownership, reset, or
remote-read behavior. It covers:

- `CVariable`
- `CPoint_data_t`
- `CHalf_edge_data`
- `CCorner_data`
- `ParentBounInfo`
- topology fields (`m_edata`, `init_node_coords`, `face_neighbors`, `face_num`)
- AMR-transfer buffers inside `CVariable`
- frozen `Nodal::CellNodalData`

Unknown fields are marked `unknown`; that label is evidence to investigate, not
permission to change.

## 2. Method and Evidence

Evidence was collected by reading the production headers and callbacks listed
below and by re-running the existing ABI micro-gate:

- `src/defines.h`
- `src/variable.h`
- `src/core/vector_matrix.h`
- `src/nodal/nodal_storage.h`
- `src/init/initializer.h`
- `src/amr/amr_callbacks.h`
- `src/amr/amr_transfer.h`
- `src/amr/amr_controller.h`
- `src/hydro/hydro_callbacks.h`
- `src/hydro/hydro_controller.h`
- `src/solver/hydro_callbacks.h`
- `src/solver/riemann_phases.h`
- `src/solver/hydro_phases.h`
- `src/mesh/ghost_session.h`
- `python/test_raw_payload_traits.py`

Existing audit docs were cross-checked but not treated as authoritative where
the current source differs:

- `docs/nodal-refactor-b2-lifecycle-audit.md`
- `docs/nodal-refactor-b3-memory-budget.md`
- `docs/nodal-refactor-contract.md`

The ABI micro-gate was rerun for this package:

```text
C:/msys64/ucrt64/bin/python.exe python/test_raw_payload_traits.py --summary .tmp/m10l0-raw-abi-summary.json
MG-RAW-ABI PASS
```

The current source defines `quad_data_t` as a fixed-size aggregate that embeds
`Nodal::CellNodalData`:

```cpp
typedef struct quad_data {
  CCorner_data m_cndata[CNDIM];
  CEdge_data m_edata[CNDIM];
  CPoint_data_t points[CNDIM];
  double init_node_coords[CNDIM][P4EST_DIM];
  ParentBounInfo m_pc_edge_data[CNDIM];
  CVariable m_vara;
  int face_neighbors[2 * CNDIM];
  int face_num;
  Nodal::CellNodalData nodal;
} quad_data_t;
```

## 3. ABI Inventory

Sizes and offsets below are the production compiler ABI
(`C:/msys64/ucrt64/bin/g++.exe`, `-O2 -g -Wall -std=c++14`). The authoritative
compile-time assertions are in `python/test_raw_payload_traits.py`.

| Type | Bytes | Notes |
|---|---:|---|
| `CDoubleVector` | 16 | two `double`, user-provided constructors |
| `CDoubleMatrix` | 32 | four `double`, user-provided constructors |
| `CPointBounInfo` | 80 | `enumType`/`Val`/`Ncp`/`Lcp`/`delta_u_cp`/`Uc_cur`/`Zc` |
| `CPoint_data_t` | 384 | four corner-point records |
| `CHalf_edge_data` | 104 | corner-edge geometry/impedance record |
| `CCorner_data` | 208 | two `CHalf_edge_data` |
| `CEdge_data` | 4 | boundary edge type |
| `ParentBounInfo` | 104 | parent-face topology/flux record |
| `CVariable` | 2992 | fixed arrays of scalar/vector/matrix fields |
| `quad_data_t` | 7232 | current full record |

Selected `quad_data_t` offsets from the ABI micro-gate:

| Member | Offset |
|---|---:|
| `m_cndata` | 0 |
| `m_edata` | 832 |
| `points` | 848 |
| `init_node_coords` | 2384 |
| `m_pc_edge_data` | 2448 |
| `m_vara` | 2864 |
| `face_neighbors` | 5856 |
| `face_num` | 5888 |
| `nodal` | 5896 |

`alignof(quad_data_t) == 8`.

The current records are raw-byte eligible under the existing
`std::is_standard_layout && std::is_trivially_copyable &&
std::is_trivially_destructible` assertion. That does not prove strict C++14
object lifetime across p4est's raw allocator; the separate M10L.3 research gate
records that question.

## 4. Allocation, Copy, and Free Routes

### 4.1 Initial forest allocation

`main.cpp` creates the forest with `p4est_new_ext(..., sizeof(quad_data_t),
Initializer::Lagrangian_init_condition, ...)`. p4est allocates raw
`sizeof(quad_data_t)` bytes per leaf and does not run a `quad_data_t`
constructor.

`Initializer::Lagrangian_init_condition` then writes:

- `nodal` reset via `Nodal::reset_storage(data->nodal)`;
- `init_node_coords` from connectivity;
- `m_vara` corner coordinates, lag corners, initial cell/thermo fields;
- `generate_children_info_from_parent`.

It does not byte-reset every field or padding byte.

### 4.2 Refine and coarsen replacement

`AMRCallbacks::Lagrangian_replace_quads` receives raw `quad_data_t*` records.
For coarsen it calls `AMRTransfer::coarsen_children_to_parent`. For refine it
resets each child's `nodal`, copies most `m_vara` fields, distributes the
parent's `ChildrenCnGeomVara` / `ChildrenPhysicalVara` buffers, and calls
`generate_children_info_from_parent`.

No placement-new, copy/move construction, or destructor is used.

### 4.3 Balance and partition

`AMRController::execute_amr` calls `p4est_balance_ext` with the same replace
callback. `execute_partition` calls `p4est_partition`. Both move raw payload
bytes according to p4est's data-size contract and do not run C++ lifetime
operations.

### 4.4 Ghost buffer

`GhostSession::initialize` allocates a raw `quad_data_t` buffer with
`P4EST_ALLOC(quad_data_t, ghost_->ghosts.elem_count)` and then calls
`p4est_ghost_exchange_data`, which copies raw `sizeof(quad_data_t)` records.

### 4.5 Destruction

`p4est_destroy` and `GhostSession::destroy` release raw storage without running
nested C++ destructors. No owned resource exists in the current record.

## 5. Field-Group Contracts

Legend for `Remote`:

- `local`: the field is meaningful only on the owning leaf.
- `remote`: the field is read from a ghost snapshot after exchange.
- `mixed`: the field is written locally and later read from local and/or remote
  records in the same callback phase.
- `unknown`: not enough current evidence to classify.

### 5.1 `CHalf_edge_data` / `CCorner_data` (`m_cndata`)

| Field | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| `enumBYD`, `BYDVal` | `quadrant_get_BYD_callback` (`HydroCallbacks`) | boundary/matrix assembly; `quadrant_corner_matrix_assemble_callback` | default constructor value, `quadrant_get_BYD_callback` | remote |
| `Rcp`, `Lcp`, `Ncp` | `quadrant_compute_RcpLcpNcp_callback`; coarse hanging `Lcp` corrected in AMR parent-edge init | corner matrix assembly; AMR hanging discovery; nodal geometry mirror | re-computed before each use | remote |
| `Zcp`, `delta_u_cp`, `Uc_cur`, `pi` | `quadrant_compute_RcpLcpNcp_callback` | corner matrix/RHS assembly; parent-edge matrix; VTU output reads `hdata[0].pi` | re-computed before each use | remote |
| `is_hanging`, `which_face` | no production writer found | no production consumer found | constructor default | unknown |

`m_cndata` is consumed by local matrix assembly, corner-to-point assembly, AMR
children-hanging discovery, hanging-point assembly, nodal boundary/geometry
mirror, and VTU corner-pressure output.

### 5.2 `CPoint_data_t` (`points`)

| Field | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| `IsHanging` | AMR `quadrant_get_children_hanging_info_callback`; hydro hanging assembly | parent-edge initialization, hanging solve, diagnostics | `quadrant_reset_hanging_info_callback` after balance | remote |
| `TwoBouns[2]` | AMR/hydro hanging discovery | corner solve (`boundary_node_velocity`), parent-edge init | overwritten per use | remote |
| `MatrixP`, `RHS` | corner-to-point matrix assembly; hanging-point matrix assembly | corner solve; relaxed hanging solve | overwritten per use | remote |
| `velo_lag` | `quadrant_corner_velocity_callback` | copy to `idcnVelocity_lag`; later solve consumers | overwritten per use | local |
| `BounParent`, `master_coord_relaxed`, `hanging_coord` | hanging-point assembly | no production reader found | unknown | unknown |
| `pi_constrained_parent` | no production writer found | AMR parent-edge init reads to produce `ParentPIStar` | unknown | unknown (unproduced read) |
| `PI_hanging`, `add_dissipation_child1`, `add_dissipation_child2` | no production producer or consumer found | no production consumer found | unknown | unknown |
| `AddDiss`, `add_dissipation_parent` | reset by `quadrant_reset_hanging_info_callback`; no producer found | no production consumer found | reset after balance | unknown |

### 5.3 `ParentBounInfo` (`m_pc_edge_data`)

| Field | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| `IsParentChildBoun` | AMR parent-edge init; relaxed hanging solve | parent-edge matrix; momentum/work callbacks; nodal geometry mirror | `quadrant_reset_parent_edge_callback` | remote |
| `Lcp[2]`, `Ncp[2]` | AMR parent-edge init | parent-edge matrix; parent-edge force | `quadrant_reset_parent_edge_callback` | remote |
| `ParentPIStar` | AMR parent-edge init from `points[].pi_constrained_parent` | no later production reader found | `quadrant_reset_parent_edge_callback` does not clear it | unknown |
| `Hanging_velocity` | AMR parent-edge init; relaxed hanging solve | parent-edge matrix; momentum/work callbacks | `quadrant_reset_parent_edge_callback` does not clear it | remote |
| `FluxRelaxed` | relaxed hanging solve | momentum/work callbacks | `quadrant_reset_parent_edge_callback` clears it | remote |
| `addDiss` | relaxed hanging solve | no production consumer found | `quadrant_reset_hanging_info_callback` clears it | unknown |
| `Zcp` | `quadrant_parent_edge_matrix_callback` | parent-edge matrix consumer | overwritten per use | local |

### 5.4 `CVariable`

`CVariable` is a fixed-array class with accessors. It contains:

- `DouCData[idDoubleCellVariableNum]`
- `DouCnData[idDoubleCornerVariableNum][CNDIM]`
- `DouEData[idDoubleEdgeVariableNum][CNDIM]`
- `IntCData[idIntCellVariableNum]`
- `VecCData[idVectorCellVariableNum]`
- `VecCnData[idVectorCornerVariableNum][CNDIM]`
- `VecEdata[idVectorEdgeVariableNum][CNDIM]`
- `ChildrenCnGeomVara[2][CNDIM][CNDIM]`
- `ChildrenPhysicalVara[2][CNDIM]`
- `MarCnData[idcnMatrixNum][CNDIM]`

The table below groups fields by producer phase rather than listing every enum.

| Group | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| initial cell thermo/state (`mass`, density, pressure, energy, volume, sound speed, gamma, centroid vectors) | `Initializer::Lagrangian_init_condition`; AMR coarsen/refine transfer | timestep prediction, AMR criteria, hydro phases | re-initialized per AMR transfer | remote |
| half-time cell state (`*_half`) | `quadrant_compute_halftime_variable_callback` | momentum/energy updates | recomputed each stage | remote |
| corner geometry/velocity (`idcnCoords_*`, `idcnVelocity_*`) | init, AMR transfer, coordinate update, corner solve copy | Riemann assembly, divergence, output | recomputed/transferred | remote |
| reconstruct pressure/density/velocity (`idReconstruct*`) | half-time variable callback | `quadrant_compute_RcpLcpNcp_callback`, corner force | recomputed each stage | local |
| corner matrix/RHS (`MarCnData[idcnMcp]`, `idcnRHS`, `ideMcp`, `ideRHS`) | corner matrix assembly, parent-edge matrix | corner-to-point assembly, hanging aggregation | overwritten each stage | remote |
| force/flux (`idcnFcp`, `idAWFcp`, `ideFcp`, `idcnFluxRelaxed`) | `quadrant_flux_relaxed_reset_callback` resets; corner force and hanging solve write | momentum/work consumers | reset at start of `RiemannSolver` | local/remote mixed |
| divergence, kinetic variation, total work | hydro volume-update phases | energy update | reset/recomputed each stage | local |
| AMR transfer buffers (`ChildrenCnGeomVara`, `ChildrenPhysicalVara`) | coarsen transfer writes parent buffers | refine transfer distributes to children | overwritten at each coarsen | local |
| coarse/refine tags (`idCoarseningTag`, `idAllowCoarsening`, `idAllowRefining`) | AMR tag/transfer callbacks | AMR error criteria | overwritten per AMR cycle | local |
| `idCDensityGradient`, `idCPressureGradient`, `idCVorticity` | AMR criteria writes (per current callbacks) | AMR criteria | recomputed per criteria | local |

`CVariableRisize` is declared but has no production definition; the field
values are instead written by the callbacks above.

### 5.5 Topology and Raw-Geometry Fields

| Field | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| `m_edata` (`CEdge_data`) | `quadrant_get_BYD_callback` writes `EdgeType` | no production reader found | overwritten per boundary pass | unknown |
| `init_node_coords` | `Initializer::Lagrangian_init_condition`; also rewritten by `quadrant_get_BYD_callback` | init writes `idcnCoords_cur`; boundary mirror reads coordinate extremes | overwritten at init/rebuild | local |
| `face_neighbors` | no production writer or reader found | no production consumer found | unknown | unknown |
| `face_num` | no production writer or reader found | no production consumer found | unknown | unknown |

### 5.6 `Nodal::CellNodalData` (`nodal`)

`Nodal::CellNodalData` is a fixed-size scalar storage block:

| Field | Last local writer | First reader | Resetter | Remote |
|---|---|---|---|---|
| `stage` (`StageStamp`) | `quadrant_stage_reset_callback` | `quadrant_validate_stage_callback`; nodal runtime guards | `Nodal::reset_storage` / stage reset | local |
| `faces`, `boundaries` | nodal boundary/geometry mirror callbacks | nodal runtime checks and validators | `Nodal::reset_storage` at init/refine/coarsen | local/remote mixed |
| `master`, `hanging`, `condensed`, `solved`, `evaluated` | `quadrant_write_local_master_callback` and related nodal runtime writes | nodal validators / diagnostics | `Nodal::reset_storage` at init/refine/coarsen | local |

The nodal storage is currently frozen under the DBGF deferral and is not part
of this refactor's numerical migration.

## 6. Lifecycle Validity Matrix

`yes` means there is a production path that writes/updates the field at that
boundary; `no` means no production path was found; `partial` means the path
exists for part of the record; `n/a` means the field is not expected there.

| Field group | Create | Refine | Coarsen | Balance | Partition |
|---|---|---|---|---|---|
| `m_cndata` | yes (init + BYD) | partial (transfer + Rcp pass) | partial (transfer) | partial (refresh + Rcp pass) | partial |
| `points` | partial | partial | partial | reset flags, matrix/RHS rewritten later | partial |
| `m_pc_edge_data` | partial | partial (parent-edge init) | partial | reset flags + parent-edge init | partial |
| `CVariable` | yes (init) | yes (transfer + buffer distribute) | yes (coarsen transfer) | partial (post-balance refresh) | partial |
| `init_node_coords` | yes | no direct writer | no direct writer | rewritten by BYD pass | no direct writer |
| `m_edata`, `face_neighbors`, `face_num` | partial | no | no | no | no |
| `nodal` | yes (`reset_storage`) | yes (`reset_storage` + stage reset) | partial (`reset_storage` in replace path for parents?) | stage reset/invalidate | stage reset/invalidate |

`partial` on partition/balance means p4est moves the raw bytes but no dedicated
payload reset exists before the next producer pass; validity is therefore only
as strong as the next stage's producer order.

## 7. Exchange Wire Format and Remote-Read Contract

The current ghost exchange always publishes the complete `quad_data_t` record:

```cpp
p4est_ghost_exchange_data(forest_, ghost_, data_);
```

`GhostSession::remote()` returns `const quad_data_t &`; some legacy callbacks
cast the returned record to a mutable pointer and rely on `!is_ghost` guards
before writing. Those write guards are not const-correctness proof and are a
known audit item for later packages.

Known exchange boundaries and first remote reader:

1. `HydroController::advance_single_stage`: after local half-time state,
   `CalculateCornerRcpLcpNcp`, nodal boundary mirror, and nodal local master,
   the session exchanges. The first remote reader after this exchange is
   `AMRCallbacks::Get_AMR_BDY_info`, which reads `m_cndata`, `points`, and
   `m_vara` from local and ghost records.
2. `MatrixAssemble`: after local corner matrix assembly the session exchanges;
   `quadrant_corner_to_point_matrix_assemble_callback` reads remote corner
   matrix/RHS state.
3. `ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes`:
   exchanges after parent-edge matrix assembly, then reads remote hanging
   point state; exchanges again after hanging point assembly, then reads
   remote relaxed hanging solve state.
4. `ComputeCornerNodeVelocity`: reads remote corner matrix/RHS state in
   `quadrant_corner_velocity_callback`.
5. AMR cycle: after refine/coarsen/balance/partition the `GhostSession` is
   invalidated and rebuilt before remote reads.

Because each exchange publishes the complete record, the "last local writer to
first remote reader" contract is per-field-group, not a compact wire schema.

## 8. Unknowns and Next Evidence

- `points[].pi_constrained_parent` is read by AMR parent-edge init but has no
  production writer found. This is the confirmed M10L.2 target.
- `points[].BounParent`, `master_coord_relaxed`, `hanging_coord`,
  `PI_hanging`, `add_dissipation_child1`, `add_dissipation_child2` are written
  or unused without a confirmed production reader.
- `m_edata`, `face_neighbors`, and `face_num` have no production consumer found
  in the current search.
- `ParentBounInfo::ParentPIStar` is written but has no later production reader.
- `ParentBounInfo::Zcp` is written by the parent-edge matrix callback and read
  by the same callback consumer path.
- Strict C++14 lifetime is deferred to M10L.3; this ledger records the
  raw-byte model without asserting lifetime safety.

## 9. Closure Record

Package: M10L.0
Base commit: `ed78788`
Hypothesis: The current complete `quad_data_t` is the canonical fixed-size
leaf payload; ownership, reset, and exchange contracts can be enumerated before
refactoring.
Allowed files: `docs/m10l0-leaf-payload-contract-ledger.md`,
`docs/legacy-optimization-refactor-taskbook.md`, `docs/README.md`
Read/write/exchange effect: read-only audit; no production or reference change.
Focused verification: ABI micro-gate `MG-RAW-ABI` PASS; ledger produced.

```text
package: M10L.0
base commit: ed78788
focused verification: MG-RAW-ABI PASS; ledger produced
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
git diff --check: PASS (LF/CRLF warning on docs/README.md only)
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged (17 reference files, no diff)
closure commit: this package's single HEAD commit (hash recorded in final handoff)
```
