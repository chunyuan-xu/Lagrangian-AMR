# M10L.11 - Cold-Field Candidate Ledger

## Purpose

This ledger records producer, reader, reset, and serialization evidence for
cold field candidates. It labels each as retain, initialize, deprecate, or
delete-later. No field is deleted or relaid out here.

All fields below are part of the complete `quad_data_t` wire record and are
therefore serialized/exchanged until removed.

## Candidates

| Field | Producer | Reader | Reset | Serialization | Label |
|---|---|---|---|---|---|
| `face_neighbors[8]` | none found | none found | none found | full record | delete-later |
| `face_num` | none found | none found | none found | full record | delete-later |
| `CEdge_data::EdgeType` (`m_edata`) | `quadrant_get_BYD_callback` | none found | overwritten per boundary pass | full record | deprecate |
| `CPoint_data_t::PI_hanging` | none found | none found | none found | full record | delete-later |
| `CPoint_data_t::add_dissipation_child1` | none found | none found | none found | full record | delete-later |
| `CPoint_data_t::add_dissipation_child2` | none found | none found | none found | full record | delete-later |
| `CPoint_data_t::AddDiss` | none found | none found | `quadrant_reset_hanging_info_callback` | full record | deprecate |
| `CPoint_data_t::add_dissipation_parent` | none found | none found | `quadrant_reset_hanging_info_callback` | full record | deprecate |
| `CPoint_data_t::BounParent` | hanging-point assembly | none found | none found | full record | deprecate |
| `CPoint_data_t::master_coord_relaxed` | hanging-point assembly | none found | none found | full record | deprecate |
| `CPoint_data_t::hanging_coord` | hanging-point assembly | none found | none found | full record | deprecate |
| `CHalf_edge_data::is_hanging` | none found | none found | constructor default only | full record | delete-later |
| `CHalf_edge_data::which_face` | none found | none found | none found | full record | delete-later |
| `ParentBounInfo::ParentPIStar` | `quadrant_set_init_parent_edge_callback` | none found | not cleared | full record | deprecate |
| `ParentBounInfo::addDiss` | relaxed hanging solve? (writer path unclear) | none found | `quadrant_reset_hanging_info_callback` | full record | deprecate |
| `ParentBounInfo::Zcp` | `quadrant_parent_edge_matrix_callback` | same callback consumer path | overwritten per use | full record | retain |

## Notes

- `pi_constrained_parent` was repaired in M10L.2 and is now initialized before
  its only reader; it remains a candidate for delete-later if `ParentPIStar`
  is later removed.
- `FluxRelaxed` is retained and has an active read/write contract after
  M10L.4-M10L.8.
- No-reader probes were performed with repository-wide `rg` searches over
  `src` excluding `src/nodal/**`; if a reader appears later, the label must be
  revisited before any deletion package.

## Gate Closure

```text
package: M10L.11
base commit: 4b645dc
focused verification: cold-field evidence ledger produced
G0: reused from M10L.10 clean build PASS
G1: reused from M10L.10 serial golden PASS
G3: reused from M10L.10 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
