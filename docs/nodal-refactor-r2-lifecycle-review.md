# R2 Raw-Storage Recovery Review

## Scope

R2 is the independent review required after the third B3b2 production-hook
attempt produced the same four-rank Sedov drift class.  It does not replace
the B2 audit or the D4b strict C++14 lifetime closure.  Its purpose is to find
concrete undefined or indeterminate reads that can make layout-only changes
perturb the legacy solver, then close them one field family at a time.

## Raw-storage evidence

- `p4est_new_ext` obtains quadrant user data from `sc_mempool_alloc` and calls
  the init callback on that storage.  The pool does not promise zero-filled
  bytes.
- Dynamic refine, coarsen, and balance pass a null init callback in
  `src/amr/amr_controller.h`; the replace callback is the only initializer for
  new child or parent payloads.
- Partition and ghost exchange move `data_size` bytes.  Forest and ghost
  destruction free storage without running one C++ destructor per record.
- G0/G1/G3 prove build and observed golden behavior.  They do not execute a
  complete object-lifetime fixture and cannot prove strict C++14 lifetime.

Consequently, a whole-record placement-new change is still prohibited here.
It would have to cover every allocation, transfer, exchange, reuse, and
destruction boundary atomically, which is the deferred D4b work order.

## R1 finding

The first confirmed indeterminate read was initial lag geometry:

- the initializer read lag corner coordinates before assigning them;
- all nine implemented `PhysicalAlg::InitCondition` cases assigned the
  current centroid but not the lag centroid.

R1 now seeds lag corners from current corners before the first read and assigns
the lag centroid immediately after each implemented case assigns the current
centroid.  Unsupported cases were not given an unconditional read.  R1 and a
separate R1 stability run both passed G0/G1/G3.

## R2.1 byte-copy traits

`CVariable` declared a constructor and destructor that had no production
definition.  The destructor declaration alone made `CVariable` and the
enclosing `quad_data_t` non-trivially-copyable and non-trivially-destructible,
despite the production program moving both as bytes.

R2.1a removed only those two declarations.  It did not remove the legacy
default constructors from `CPointBounInfo`, `CHalf_edge_data`, or
`ParentBounInfo`.  `MG-RAW-ABI` now checks:

- standard-layout, trivially-copyable, and trivially-destructible traits for
  the six target records, `CVariable`, and `quad_data_t`;
- frozen size, alignment, and critical `quad_data_t` offsets;
- rejection of a deliberately non-trivially-destructible fixture;
- a `memcpy` round trip between two live `quad_data_t` objects.

The micro-gate explicitly records
`strict_cxx14_raw_storage_lifetime_proved=false`.  Traits make the existing
byte-copy contract less internally contradictory; they do not start object
lifetime in storage returned by the p4est/sc allocator.

## R2.1 inactive parent-edge read

`quadrant_compute_corner_force_callback` previously evaluated
`Hanging_velocity`, `Lcp`, `Ncp`, and `ideMcp` for all four faces, including
faces where `IsParentChildBoun` was false.  Downstream momentum and work
consumers already guard `ideFcp` with that active flag, so the inactive
calculation had no authorized numerical consumer.

R2.1b introduced a pure parent-edge force kernel first, with no production
include or call.  Its focused gate proves:

- an inactive record poisoned with NaNs returns a finite zero force without
  consulting inactive geometry;
- an active record is bitwise identical to the legacy expression.

Only after that anchor passed G0/G1/G3 was the single callback loop connected
to the kernel.  The connected callback then passed its own G0/G1/G3.

## Evidence

| Slice | Focused evidence | Full gate artifact | Result |
|---|---|---|---|
| R1 | `python/test_initial_lag_geometry.py` | `.tmp/gates/R1-20260824-052958-c903fc03d11e4ba78942c84886a83899` | PASS |
| R1 stability | same production state | `.tmp/gates/R1STABILITY-20260824-053906-f8e23a7f92af4ce7bf6e70db94930308` | PASS |
| R2.0 expected red | `MG-RAW-ABI` rejects `CVariable` and `quad_data_t` | `.tmp/gates/R2-0-RED-20260824-055819` | expected FAIL |
| R2.1a | 30-assertion `MG-RAW-ABI` | `.tmp/gates/R2-1A-20260824-060041-b32f23deffb1406185f0be75ad7b15a1` | PASS |
| R2.1b1 | inactive poison and active parity fixtures | `.tmp/gates/R2-1B1-20260824-060615-f932378a051448489f7c3b6f161f6b1e` | PASS |
| R2.1b2 | same micro-gate after one callback connection | `.tmp/gates/R2-1B2-20260824-061031-db08a7cf53f14b4cbf20c8ba7da07d2a` | PASS |

Every passing full-gate summary records G0/G1/G3 PASS and all four restoration
flags as true.  No reference or tolerance was changed.

## Deferred lifecycle fixtures

Before D4b can claim strict lifecycle closure, independent fixtures remain
required for:

1. initial `p4est_new_ext` allocation and callback coverage;
2. refine-created children;
3. coarsen-created parents;
4. balance-created children;
5. multi-rank partition byte preservation;
6. real ghost exchange and rebuild generation;
7. mempool reuse across refine/coarsen/refine;
8. destruction of every storage owner.

Each fixture must report exact event/assertion counts and must be independently
gated.  A fake ghost exchange or a final VTU comparison is not sufficient
evidence for these entries.

## Decision

R2 closes the two concrete perturbation sources above and establishes a
defensible byte-copy trait gate.  It does not authorize broad legacy reset,
placement-new, or a strict C++14 lifetime claim.  B3b diagnostic integration
may be reconsidered only from the last passing disabled D3 anchor, one call
category at a time, with default-off G0/G1/G3 before any enabled measurement.
Any recurrence of the Sedov drift immediately re-blocks that route.
