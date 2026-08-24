# B3: Double-Layout Memory Budget

## Measured current record sizes

Measured for B3a on 2026-08-24 with the production compiler declared by the
Makefile, `C:/msys64/ucrt64/bin/g++.exe` (MSYS2 UCRT64 GCC 16.2.0, x86_64),
using `-O2 -g -Wall -std=c++14` and the production source, p4est, MS-MPI, and
UCRT include paths.  The probe source is `.tmp/size_probe.cpp`; its SHA-256
is `41D1495AEFF4F9D7139D929230BCCF6768C2F17CD45F11FE192540D5E6F69C2B`.
The source, output, and full compiler log are archived with the B3a gate
evidence.  Sizes are in bytes:

| Type | Bytes |
|---|---:|
| `quad_data_t` | 5896 |
| `CVariable` | 2992 |
| `CPointBounInfo` | 80 |
| `CPoint_data_t` | 384 |
| `CHalf_edge_data` | 104 |
| `CCorner_data` | 208 |
| `CEdge_data` | 4 |
| `ParentBounInfo` | 104 |
| `CDoubleVector` | 16 |
| `CDoubleMatrix` | 32 |

## Provisional DBGF payload

The provisional fixed-size scalar POD layout used for budgeting is:

| Type | Bytes |
|---|---:|
| `StageStamp` | 24 |
| `EdgeSegmentGeometry` | 40 |
| `FaceData` (2 segment geometries) | 88 |
| `CellMasterContribution` | 192 |
| `CellHangingContribution` | 192 |
| `AggregatedHangingContribution` (stack-only) | 192 |
| `CondensedMasterContribution` | 192 |
| `MasterSolveState` | 64 |
| `EvaluatedCellFlux` | 160 |
| `CellNodalData` total | 1176 |

This is a conservative first-pass size: every corner stores a full matrix,
right-hand side, hanging contribution, condensed contribution, solved
velocity, and evaluated force/power.  T1 may reduce it later, but B3 freezes
the budget upper bound, not the final size.

The measured `CellNodalData` decomposition is:

| Field group | Count x element bytes | Payload bytes |
|---|---:|---:|
| stage stamp | `1 x 24` | 24 |
| four fixed-capacity faces | `4 x 88` | 352 |
| local master contribution (`M`: 128, `b`: 64) | `1 x 192` | 192 |
| local hanging contribution (`M`: 128, `b`: 64) | `1 x 192` | 192 |
| condensed endpoint ledger (`M`: 128, `b`: 64) | `1 x 192` | 192 |
| solved corner velocities | `1 x 64` | 64 |
| evaluated branch/physical forces and power | `1 x 160` | 160 |
| **total** |  | **1176** |

`AggregatedHangingContribution` is a distinct 192-byte type but is a
face-callback stack temporary, so it is intentionally excluded from the
per-leaf total.  A `FaceData` contains a 4-byte logical header, 4 bytes of
alignment padding, and two 40-byte segment geometries.  These are projection
types only; T1 freezes the actual definitions and must re-run layout gates.

## Double-layout projection

| Case | Current record | Proposed record | Increase |
|---|---:|---:|---:|
| per leaf | 5896 B | 7072 B | +1176 B (+19.9%) |

Using the reference VTU cell counts as the largest leaf count evidence:

| Configuration | Max cells / rank | Current leaf payload | Proposed leaf payload |
|---|---:|---:|---:|
| Serial Sedov AMR | 5422 | ~31.9 MB | ~38.3 MB |
| 4-rank Sedov AMR | 1361 | ~8.0 MB | ~9.6 MB |
| 4-rank Sod AMR | 1111 | ~6.5 MB | ~7.9 MB |

Ghost payload depends on connectivity and partition.  Even assuming a very
conservative ghost count equal to the local count, the proposed record
remains below a per-record ceiling of 8192 bytes and adds at most ~1.2 KB
per leaf/ghost.

## Provisional capacity contract

1. `sizeof(quad_data_t)` after embedding `CellNodalData` must stay below
   `8192` bytes unless a separate machine-memory review approves a higher
   ceiling.
2. `CellNodalData` must be fixed-size scalar POD with no pointers.
3. If the actual peak exceeds the ceiling, the DBGF payload must be split
   into separate per-face/per-corner groups rather than increasing the
   ceiling.
4. The final layout may be smaller, but the first double-layout milestone
   must not exceed this projection before a focused memory measurement.

## Measured dynamic high-water (B3b3)

Versioned per-rank JSON and aggregate JSON are written only when
`LAGRANGIAN_MEMORY_HIGH_WATER=1`; canonical runners scrub the variable.
Measured maxima below are the aggregate values recorded by
`B3B3-ENABLED` on 2026-08-25.  `coverage_union_bits=7` in all runs,
meaning initial, ordinary, and rebuild exchange categories were observed.

| Configuration | Max local leaves | Max ghost leaves | Max local payload | Max ghost payload | Max send / receive payload |
|---|---:|---:|---:|---:|---:|
| Serial Noh Uniform | 1024 | 0 | 6.0 MB | 0 | 0 / 0 |
| Serial Sod AMR | 4678 | 0 | 27.6 MB | 0 | 0 / 0 |
| Serial Sedov AMR | 5518 | 0 | 32.5 MB | 0 | 0 / 0 |
| 4-rank Sod AMR | 1266 | 169 | 7.5 MB | 1.0 MB | 1.0 MB / 1.0 MB |
| 4-rank Sedov AMR | 1420 | 218 | 8.4 MB | 1.3 MB | 1.3 MB / 1.3 MB |

The serial values match the double-layout projection closely enough to keep
the proposed 7072-byte record below the 8192-byte ceiling: the largest
observed serial local payload is about 32.5 MB (5518 leaves), and the
four-rank Sedov per-rank payload is about 8.4 MB.  The cumulative send and
receive bytes recorded for the enabled four-rank runs are the sum of the
per-rank cumulative counters and are not a per-step peak.

## Status

**B3a status:** the current and projected layouts have been measured with the
production compiler, the field-level projection is recorded, and the proposed
record size is 7072 bytes, 1120 bytes below the hard ceiling.  B3a does not
freeze the final operational budget.

**B3b status:** the opt-in read-only high-water instrumentation is connected
to initial, ordinary, and rebuild exchange categories and writes versioned
per-rank JSON plus an aggregate JSON for serial and four-rank Sod/Sedov.
`B3B3-DEFAULT` and `B3B3-ENABLED` both passed G0/G1/G3; the enabled gate
recorded the table above.  B3b is closed.

**B3c status:** the operational budget is frozen from the measured B3b3
results: `sizeof(quad_data_t) <= 8192` bytes, with the current measured record
at 5896 bytes and the first double-layout projection at 7072 bytes.  The
temporary high-water probe is retained behind the disabled-by-default
`LAGRANGIAN_MEMORY_HIGH_WATER` diagnostic switch and never writes output in
canonical runs.  B3c is closed; B3 is complete.
