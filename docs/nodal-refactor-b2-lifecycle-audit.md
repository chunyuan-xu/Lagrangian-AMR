# B2: Whole-Record Raw-Byte Lifecycle Audit

## Purpose

The DBGF payload will be embedded in `quad_data_t`, which is allocated and
moved by p4est as raw bytes.  Before adding any field, every object-lifetime
entry point must be understood.  This audit does not change production code.

## 1. Allocation and exchange entry points

### 1.1 Initial p4est forest

`src/main.cpp` creates the forest with:

```cpp
p4est_new_ext(mpicomm, conn, 1, startup_config.mesh.minimum_level, 1,
              sizeof(quad_data_t),
              Initializer::Lagrangian_init_condition,
              (void *)(&bridge));
```

`p4est_new_ext` allocates `sizeof(quad_data_t)` bytes per quadrant through
p4est's raw allocator.  It does not call the C++ default constructor of
`quad_data_t` or of its nested members.  In strict C++14 terms this is raw
storage used through typed pointers; it must not be justified using C++20
implicit-lifetime rules.

`Initializer::Lagrangian_init_condition` is passed as the init callback for
the initial forest and for quadrants created by the startup
`p4est_balance`.  It writes coordinates, selected velocity and thermodynamic
fields, and parent-to-child buffers.  It does not byte-reset every field or
padding byte in `quad_data_t`; therefore its presence is not a whole-record
lifetime or initialization proof.

### 1.2 Ghost buffer

`src/mesh/ghost_session.h` allocates the ghost mirror with:

```cpp
data_ = P4EST_ALLOC(quad_data_t, ghost_->ghosts.elem_count);
```

Again, no C++ constructor runs.  `exchange()` calls
`p4est_ghost_exchange_data`, which copies raw `quad_data_t` records.

### 1.3 AMR replace callback

`src/amr/amr_callbacks.h` (M9.1.3 `Lagrangian_replace_quads`) receives raw
`quad_data_t*` for incoming/outgoing quadrants.  Refinement writes child
records and coarsening writes the parent record through raw pointers.  No
placement-new or explicit destructor is used.

### 1.4 Balance and partition movement

`AMRController::execute_amr` calls `p4est_refine_ext`, rebuilds the ghost
session, calls `p4est_coarsen_ext`, and then calls `p4est_balance_ext` with
the same replace callback.  `AMRController::execute_partition` calls
`p4est_partition`.  These p4est operations relocate or create user-data
records according to p4est's byte-oriented data-size contract; the C++ code
does not wrap those transfers in copy/move construction.  Both controller
paths invalidate the `GhostSession`; refine rebuilds before coarsen tagging,
while the post-balance and post-partition sessions are destroyed and rebuilt
by the caller before remote reads.

The startup `p4est_balance` uses the init callback for newly created
quadrants, then startup `p4est_partition` may migrate the initialized bytes.
There is no `GhostSession` yet at this point.

### 1.5 AMR refresh snapshot

`append_refresh_snapshot` snapshots every local and ghost record as raw bytes
(`sizeof(quad_data_t)` each).  This is used after balance operations to
detect unintended mutation.  The new solver storage must be deterministic
after explicit reset so such byte snapshots remain meaningful.

### 1.6 Destruction

`p4est_destroy` releases the forest and its quadrant user-data storage without
calling `quad_data_t` or nested C++ destructors.  `GhostSession::destroy`
similarly calls `P4EST_FREE(data_)` after destroying the p4est ghost; it does
not invoke one destructor per mirrored record.  This matches the existing
byte-storage behavior but is not a strict C++14 lifetime proof.

## 2. Trivial-copyability of the current record

The current record contains:

- `CCorner_data[4]`, each with `CHalf_edge_data[2]`
- `CEdge_data[4]`
- `CPoint_data_t[4]`
- `ParentBounInfo[4]`
- `CVariable`
- scalar topology fields

`CVariable` declares a user-provided destructor and a user-provided
constructor in `src/variable.h`:

```cpp
CVariable();
~CVariable();
void CVariableRisize();
```

Those definitions are not present in the production build; only focused
Python compile-test stubs define them.  Production happens to avoid odr-use
because p4est never constructs or destroys a `quad_data_t`.  That explains
the successful link but does not make the operation lifetime-safe.  A class
with a user-declared destructor is not trivially destructible, so
`quad_data_t` is **not** trivially copyable in the strict C++14 sense.

Nested helper types also have user-provided constructors:

- `CPointBounInfo()`
- `CHalf_edge_data()`
- `ParentBounInfo()`
- `CDoubleVector()`, `CDoubleVector(double, double)`
- `CDoubleMatrix()`, `CDoubleMatrix(double, double, double, double)`

These constructors are not run by p4est raw allocation.  Some fields are
only initialized by callbacks after allocation.  Uninitialized fields or
padding are therefore copied as garbage until explicitly reset.

## 3. Lifecycle boundary list

Every topology event must reset transient solver caches:

| Event | Existing invalidation | Solver cache requirement |
|---|---|---|
| `p4est_new_ext` | none; init callback writes state | reset and initialize |
| startup `p4est_balance` | init callback for new cells; no ghost yet | reset and initialize |
| startup `p4est_partition` | no ghost yet | reset owner-local transient state |
| dynamic `p4est_balance_ext` | GhostSession invalidated/destroyed | reset face/corner caches |
| dynamic `p4est_partition` | GhostSession invalidated/destroyed | reset owner/ghost caches |
| refine | `Lagrangian_replace_quads` | reset child transient fields |
| coarsen | `Lagrangian_replace_quads` | reset parent transient fields |
| ghost rebuild | `GhostSession::initialize` | reset ghost mirror fields |
| ghost exchange | raw `sizeof(quad_data_t)` publication | publish only initialized stages |
| refresh snapshot | raw local and ghost byte copy | all compared bytes deterministic |
| forest/ghost destruction | raw free, no nested destructors | no owned resources in new storage |
| geometry update | callback updates coords | invalidate geometry epoch |
| Riemann iteration | loop counter changes | invalidate all solver stages |
| time/substage | caller changes stage | invalidate all solver stages |

## 4. Required invariants for new storage

New solver fields must satisfy:

- fixed size, no `std::vector`/`std::string`/pointers/handles;
- no virtual functions;
- no user-declared destructor;
- scalar math storage only, with pure functions converting to/from
  `CDoubleVector`/`CDoubleMatrix`;
- explicit `reset_*()` called at every lifecycle boundary above;
- a `StageStamp` that fails closed when geometry, topology, time step,
  substage, iteration, or phase no longer matches.

## 5. Decision

Embedding new shadow fields as a **new independent scalar POD struct** is
compatible with the existing raw-byte ABI and does not add pointer or
destructor risk.  It does **not** prove strict C++14 object-lifetime safety for
the enclosing `quad_data_t`.  Adding a static_assert for the new struct is
useful evidence about the new fields only.  A whole-record lifetime strategy
must be deferred until `CVariable` and the legacy helper constructors are
removed, or explicit placement-new/destruction and lifetime wrappers cover
initial allocation, refine, coarsen, balance, partition, ghost allocation,
exchange, and destruction.
