# DBGF Nodal Solver Refactor Execution Taskbook

## 1. Purpose

This is the executable work order for migrating the current per-leaf nodal
solver to the DBGF design in `docs/coupled_nodal_solver.md`.  It is written
for a C++ implementation agent.  Follow the milestones in order.  Do not
skip a gate, combine milestones, or update golden assets to hide a failure.

The production target is corrected design B:

- each leaf owns fixed-capacity corner and face records;
- local and condensed contributions have different C++ types;
- aggregated hanging `M_h,b_h` are stack-local face-callback temporaries;
- the coarse-cell owner is the unique condensed-contribution writer;
- each local leaf owner recovers and writes only its own evaluated force;
- legacy and DBGF are mutually exclusive whole-step pipelines.

Design A may exist only as an optional audit buffer.  It must never be used
by momentum, work, or energy production consumers.

## 2. Authoritative inputs

Read these files before editing code:

1. `docs/coupled_nodal_solver.md`: DBGF mathematics and invariants.
2. `docs/golden-gates.md`: canonical G0/G1/G3 procedure.
3. `docs/nodal-refactor-contract.md`: legacy ownership and phase graph.
4. `docs/nodal-refactor-b2-lifecycle-audit.md`: raw-byte constraints.
5. `docs/nodal-refactor-b3-memory-budget.md`: layout ceiling.
6. `src/hydro/hydro_controller.h` and `src/solver/riemann_phases.h`: current
   phase order.
7. `src/mesh/ghost_session.h` and `src/amr/amr_callbacks.h`: ghost and AMR
   lifecycle.

If code and this taskbook disagree, stop and update the taskbook with source
evidence before implementing.  Do not silently choose one interpretation.

## 3. Global execution rules

### 3.1 One change axis

Each submilestone may change exactly one of:

- one type/field group;
- one pure mapping or algebra kernel;
- one callback writer;
- one callback reader;
- one exchange boundary;
- one lifecycle/reset boundary;
- one top-level solver selector.

Never change data layout, callback order, and numerical semantics in the same
submilestone.  L1a is the only documented exception: embedding raw wire bytes
and initializing them before their first exchange/snapshot must be atomic.

### 3.2 Authority states

| Phase | Production authority | New layout status | Golden authority |
|---|---|---|---|
| B/T | legacy | absent or inert | legacy G1/G3 |
| L | legacy | mirror and shadow-accessor compatibility | legacy G1/G3 |
| S | legacy | DBGF shadow only | legacy + micro-gates |
| E | selector; default legacy | complete experimental DBGF | legacy + candidate gates |
| A | approved DBGF | candidate then default | versioned DBGF golden |
| D | DBGF | legacy being removed | DBGF golden |
| C0 | planar boundary closure | cylinder remains disabled | no cylinder golden |

### 3.3 File ownership

Only one agent may write these core files at a time:

- `src/defines.h` and `src/variable.h`;
- `src/hydro/hydro_callbacks.h`;
- `src/solver/hydro_callbacks.h` and the phase graph;
- `src/amr/amr_callbacks.h` and AMR transfer code.

Tests and documentation may be developed in parallel only when they do not
modify the same files.  A reviewer does not edit the implementation under
review.

### 3.4 Focused anchor

Before each milestone record:

```text
milestone:
base commit:
working-tree user changes:
single hypothesis:
allowed files:
forbidden files:
legacy authority:
new authority:
expected numerical change: none/explicit
param.ini SHA-256:
reference manifest SHA-256:
```

After passing, record changed files, tests, G0/G1/G3 summary paths, solver and
comparison exit codes, parameter restoration, reference hash, and the focused
commit or diff identifier.  Do not include unrelated user files in a commit.

## 4. Mandatory gate protocol

Every code or documentation milestone runs the full gate unless the milestone
explicitly requires a stronger candidate gate.  A micro-gate supplements the
official gates; it never replaces them.

### 4.1 Preflight

From repository root in one PowerShell process:

```powershell
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
$gateTmp = 'C:\ai\Lagrangian-AMR\.tmp'
$env:TEMP = $gateTmp
$env:TMP = $gateTmp
$env:TMPDIR = $gateTmp
New-Item -ItemType Directory -Force -Path $gateTmp | Out-Null
Remove-Item Env:LAGRANGIAN_TRACE_TARGET,Env:LAGRANGIAN_TRACE_REFINE,Env:LAGRANGIAN_VERBOSE_AMR,Env:LAGRANGIAN_TRACE_CHECKSUM,Env:LAGRANGIAN_MEMORY_HIGH_WATER -ErrorAction SilentlyContinue
$pythonCandidates = @(
  'C:\Users\a9ood\AppData\Local\Python\pythoncore-3.14-64\python.exe',
  'C:\msys64\ucrt64\bin\python.exe'
)
$py = $null
foreach ($candidate in $pythonCandidates) {
  if (Test-Path $candidate) {
    & $candidate -c 'import numpy; print(numpy.__version__)' 2>$null
    if ($LASTEXITCODE -eq 0) { $py = $candidate; break }
  }
}
if (-not $py) { throw 'No Python interpreter with NumPy is available' }
```

Capture `git status --short`, `git rev-parse HEAD`, `param.ini` SHA-256, and
a sorted SHA-256 manifest for all files below `reference/`.  Freeze the
selected Python path in the milestone record.  A missing NumPy environment is
an environment failure; do not modify solver code or the Makefile to hide it.

### 4.2 G0

```powershell
& 'C:\msys64\usr\bin\make.exe' clean
if ($LASTEXITCODE -ne 0) { throw 'G0 clean failed' }
& 'C:\msys64\usr\bin\make.exe' -j1 'CXXFLAGS=-O2 -g -Wall -std=c++14 -pipe -fno-use-linker-plugin'
if ($LASTEXITCODE -ne 0) { throw 'G0 diagnostic build failed' }
& 'C:\msys64\usr\bin\make.exe' clean
if ($LASTEXITCODE -ne 0) { throw 'G0 formal clean failed' }
& 'C:\msys64\usr\bin\make.exe' -j8
if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\bin\AMR_Solver.exe)) { throw 'G0 formal build failed' }
```

### 4.3 G1 and G3

`run_tests.py` deletes the entire `output/` directory.  Before G1, resolve the
absolute repository and output paths, verify both stay inside the workspace,
then move any pre-existing `output/` directory intact to a unique
milestone-specific `.tmp/gates/<milestone>/preexisting-output/`.  Create a new
empty `output/`.  Never run G1 against user output in place.

```powershell
& $py .\python\run_tests.py
if ($LASTEXITCODE -ne 0) { throw 'G1 failed' }
& $py .\python\run_mpi_gates.py
if ($LASTEXITCODE -ne 0) { throw 'G3 failed' }
```

Run G3 in the same newly created gate-owned `output/`, after archiving or
removing only the G1 gate-owned files that could collide.  Verify every target
and piece modification time is later than the corresponding runner start.
After the gate, move the entire gate-owned output plus copies of both summary
JSON files into `.tmp/gates/<milestone>/run-artifacts/`, then restore the
pre-existing output directory exactly.  After each runner verify its JSON
schema, every case status and exit code, and `param_restored=true`.

### 4.4 Interrupted runner recovery

Before running a gate, copy `param.ini` to a milestone-specific backup below
`.tmp/` and record its hash.  If the terminal, tool call, or agent turn is
interrupted, do not resume implementation.  First:

1. confirm no `AMR_Solver` or `mpiexec` process is still running;
2. compare `param.ini` with the recorded hash;
3. restore the exact backup if they differ;
4. archive any partial gate-owned output and restore the pre-existing output
   directory if it had been isolated;
5. verify `reference/` hash and `git status`;
6. rerun the interrupted gate from its beginning.

An interrupted G3 is not PASS even if Sod completed before interruption.

### 4.5 Failure and automatic subdivision

- First failure: stop, classify environment/build/solver/comparison/topology,
  and save the full log plus a patch of the agent-owned hunks.  Revert only a
  precisely identified agent-owned committed anchor with `git revert`.  For
  uncommitted work, use an inverse `apply_patch` limited to agent-owned hunks.
  Never use `git restore`, `git checkout`, `git clean`, or `reset --hard` in a
  dirty user worktree.
- Second failure with the same root cause: the milestone is too broad.  Split
  it into submilestones by writer, reader, exchange, or lifecycle boundary.
- Third failure after subdivision: stop implementation and request an
  independent review.  Do not weaken tolerance, regenerate references, add
  node-level fallback, or combine legacy and DBGF.
- A rerun-only green result after an unexplained failure is still a failure.

### 4.6 Legacy and DBGF gate rails

The official gate names and the approval milestones are different concepts:

| Program point | Required explicit rails | Canonical alias |
|---|---|---|
| before E1a | current `run_tests.py` / `run_mpi_gates.py` | legacy |
| E1a through E1c | G1-L/G3-L explicitly select legacy plus shadow suite | legacy |
| E1d through A2b | G1-L/G3-L plus non-golden candidate case/micro-suite | legacy |
| A2c through A3a | explicit legacy and DBGF golden rails plus default-mode smoke | still legacy |
| after A3b | both explicit rails while supported | DBGF |
| D phase after legacy retirement | G1-D/G3-D | DBGF |

E1a must make the existing runners explicitly request legacy mode, record it
in run metadata, and restore every temporary parameter byte.  A2c adds
permanent explicit `run_legacy_tests.py`, `run_legacy_mpi_gates.py`,
`run_dbgf_tests.py`, and `run_dbgf_mpi_gates.py`; it does not silently repoint
the canonical runners.  At every rail, verify the binary's reported mode
matches the reference namespace.  A default-mode PASS cannot replace either
explicit rail.

## 5. Micro-gate catalog

| Gate | Required proof |
|---|---|
| `MG-LAYOUT` | `sizeof/alignof/offsetof`, fixed capacity, no pointers, trivial/standard-layout/copyable/destructible assertions for new types |
| `MG-RESET` | poison with `0xA5`, reset, then prove every readable byte/field defined and reset idempotent |
| `MG-TOPO` | four faces, two fine-side orders, corner endpoints, cross-tree orientation, invalid input fail-fast |
| `MG-GEOM-P` | planar normals, segment lengths, endpoint mapping, coarse/fine sign and full-cell closure |
| `MG-GEOM-C` | cylindrical endpoint metric weights and segment additivity only; no solver activation |
| `MG-EPOCH` | stale reads fail after geometry/stage/refine/coarsen/balance/partition |
| `MG-LOCAL` | symmetric PSD `M_ch`, `b_ch=M_ch U_c+P_c N_ch`, direct/hanging type separation |
| `MG-CONDENSE` | one global face reduction, two endpoint injections, weights nonnegative/sum one, multiple faces use `+=` |
| `MG-SOLVE` | finite matrix, boundary-aware rank/condition policy, scaled master residual, truly unconstrained singular systems fail-fast |
| `MG-FORCE` | `r_pi`, `r_F`, `r_D`, and `D_ch >= -eps` from section 15.1 |
| `MG-GCL` | fully discrete swept-volume residual |
| `MG-CONS` | global momentum minus boundary impulse and energy minus boundary work |
| `MG-MPI` | 1/2/4 ranks, `cross_rank_hanging_faces>0`, canonical node/face keys, count and ledger equality |
| `MG-AMR` | refine-only, coarsen-only, balance-only, partition-only |
| `MG-FALLBACK` | solver mode frozen at step start; no callback/node fallback |

Algebraic thresholds must be scale-aware, for example
`C*epsilon*(scale+1)`, and frozen before seeing results.

### 5.1 Micro-gate infrastructure is a deliverable

The repository does not currently contain executable runners for these named
micro-gates.  A gate name in a report is not evidence.  Before a milestone
can cite a micro-gate, create its test infrastructure as a separate
submilestone:

1. add the fixture and runner without production integration;
2. include at least one positive fixture and one deliberate invalid/negative
   fixture;
3. prove the runner passes the positive fixture and detects the negative one;
4. emit a machine-readable summary with schema, executed assertion count,
   fixture coverage count, status, and thresholds;
5. run G0/G1/G3 for the test-infrastructure submilestone;
6. only then implement the production kernel/callback that must satisfy it.

Use one registry runner such as `python/run_nodal_micro_gates.py`, with focused
compile/run helpers under `python/nodal_gates/`.  A gate reports ERROR, not
PASS, if zero assertions ran, a required fixture was skipped, or the expected
negative fixture was not rejected.

## 6. Milestone work orders

### B0 - Trusted current baseline

**Objective:** prove the exact current worktree state is a stable legacy
baseline.

**Actions:** capture an exact worktree fingerprint (commit plus tracked and
untracked user changes), environment, and hashes.  Execute the complete
`G0 -> G1 -> G3` sequence three consecutive times because Sedov MPI has
historical nondeterminism.  A clean commit alone is not the fingerprint when
the worktree is dirty.

**Forbidden:** source edits, parameter edits outside runners, reference edits,
or using historical PASS records.

**Exit:** all cases actually ran, summaries PASS, parameter/reference hashes
unchanged, and user worktree changes preserved.

### B1 - Callback and authority contract

**Objective:** inventory legacy Reads/Writes/Exchange/Invalidates relationships.

**Actions:** update `docs/nodal-refactor-contract.md` from whole-repository
search; include phase order, local/ghost ownership, stable CellKey, and all
consumers of `ParentBounInfo`, `FluxRelaxed`, point/corner/half-edge fields.

**Forbidden:** production code changes.

**Exit:** search and contract agree; G0/G1/G3 PASS.

### B2 - Whole-record lifecycle audit

**Objective:** explain every raw allocation/copy/transfer entry point.

**Actions:** audit `p4est_new_ext`, ghost allocation/exchange, init, refine,
coarsen, balance, partition, refresh snapshot, and destruction.  Record why
new storage must be independent scalar POD and why whole `quad_data_t` cannot
yet receive a trivial-copy assertion.

The permitted conclusion is “compatible with the existing raw-byte ABI and
does not add pointer/destructor risk”, not “strict C++14 lifetime safety is
proved”.  Strict lifetime closure is deferred to D4b.

**Forbidden:** embedding a new field or attempting broad legacy lifecycle
repair.

**Exit:** `docs/nodal-refactor-b2-lifecycle-audit.md` complete; G0/G1/G3 PASS.

### B3 - Double-layout memory budget

**Objective:** freeze an upper bound before embedding shadow storage.

**Submilestone B3a:** measure current and projected type sizes with the
production compiler; record field-level estimates; G0/G1/G3.

**Submilestone B3b:** add read-only, opt-in instrumentation outside the leaf
payload that records per-step/per-rank high-water values for local leaves,
ghost leaves, payload bytes, and estimated/observed exchange bytes.  Run
serial and four-rank Sod/Sedov through dynamic AMR; G0/G1/G3.

Execute B3b through the following independently gated slices.  Do not combine
their change axes:

1. **B3b1:** add the external checked high-water accumulator and focused
   arithmetic tests; do not connect it to production exchange paths.
2. **B3b2a:** declare the exchange observation/callback types and prove that
   `sizeof(GhostSession)` and its production behavior are unchanged.
3. **B3b2b:** add only borrowed observer storage to `GhostSession`; exchange
   must not read it yet.  If this layout-only slice changes a golden result,
   reject the member-based design and use an independent observed-exchange
   adapter that preserves the legacy `GhostSession` layout.
4. **B3b2c:** connect the default-null exchange hook.  Snapshot the schedule
   before exchange and notify only after exchange returns.  The null path must
   not sample memory, open files, call an MPI collective, or change exchange
   order.
5. **B3b2d:** connect the external tracker behind a disabled-by-default
   diagnostic switch; cover initial exchange, rebuild, and ordinary refresh
   exchange without introducing output serialization or rank collectives.
6. **B3b3:** add versioned per-rank CSV/JSON output and final MPI aggregation,
   then run serial and four-rank Sod/Sedov through dynamic AMR.

B3b3 closure evidence: `B3B3-DEFAULT` and `B3B3-ENABLED` gate artifacts both
PASS on 2026-08-25.  The enabled gate records versioned per-rank JSON plus an
aggregate JSON for serial and four-rank Sod/Sedov; measured maxima are recorded
in `docs/nodal-refactor-b3-memory-budget.md`.  B3b is closed; B3c remains.

The first combined B3b2 attempt is retained as a failed artifact at
`.tmp/gates/B3B2-20260824-023430-543ab90a61b9463db2499e5c7c1541ca`.
G0/G1 and four-rank Sod passed, but four-rank Sedov ended at step 4017 instead
of the reference step 3933 and had 5470 rather than 5434 cells.  All
restoration checks passed, so this is classified as a real MPI topology drift,
not gate infrastructure.  The combined implementation must not be rerun
unchanged or counted as evidence.

The direct-hook B3b2c attempt is also rejected and retained at
`.tmp/gates/B3B2C-20260824-032235-2dd9ac583e4a46c6a3b5f0b8383c0fa7`.
After B3b2a and B3b2b each passed, changing the legacy `exchange()` body made
four-rank Sedov end at step 5682.  Therefore B3b2c is further split: B3b2c1
must restore the legacy `exchange()` body exactly and add a separately invoked
free-function `exchange_observed()` adapter in a diagnostic-only header; later
diagnostic wiring may call that API, but the disabled production path must not
branch inside legacy `exchange()`.  This narrows the associated change axis but
does not establish the root cause of the existing MPI sensitivity.
After B3b2c1 closes, B3b2c2 may include the adapter from the production
simulation translation unit but must leave it uncalled; give that include-only
axis its own G0/G1/G3 before adding any diagnostic selector or observed call.
Split B3b2d further.  B3b2d0 adds only the observation-to-tracker context and
focused mapping/negative tests, with no production include or call.  Later
slices add binding accessors, free selected wrappers, the strict environment
selector/ownership scaffold, and then Initial, Ordinary, and Rebuild call sites
one category at a time.  Every slice receives its own G0/G1/G3; enabled runs
must still match the same golden outputs before their measurements are valid.
For B3b2d4, replace only the first simulation initialize with the selected
wrapper.  Canonical runners remain default-off; the explicit serial evidence
uses `python/run_tests.py --memory-high-water`, requires the same three golden
comparisons, and records the enabled mode in its JSON summary.

B3b2d4 default-off failed and is retained at
`.tmp/gates/B3B2D4DEFAULT-20260824-050002-9117441277d74963986c1e29a630f923`.
Four-rank Sedov ended at step 5702 while G0, G1, and four-rank Sod passed and
all restoration checks remained true.  That was the third recurrence of the
same MPI drift class after subdivision.

The independent root-cause audit is `docs/nodal-refactor-r2-lifecycle-review.md`
(R1/R2).  R1 fixed the initial lag-geometry indeterminate reads
(`src/init/initial_geometry.h`, `src/init/initializer.h`, `src/alg.cpp`) and
passed its own R1 and R1STABILITY gates.  R2.1a removed the undefined
`CVariable` constructor/destructor declarations and added the raw-byte trait
gate; R2.1b1/b2 added and connected the inactive-zero parent-edge force
kernel.  R2-AUDIT passed G0/G1/G3.

After those fixes the route was reopened one call site at a time:

- `B3B2D4R-DEFAULT`: first simulation initialize via the selected wrapper,
  default-off, G0/G1/G3 PASS.
- `B3B2D4R-ENABLED`: same selected wrapper under
  `LAGRANGIAN_MEMORY_HIGH_WATER=1` serial, G0/G1/G3 PASS.
- `B3B2D5-DEFAULT`: per-step ordinary refresh exchange via the selected
  wrapper, default-off, G0/G1/G3 PASS.
- `B3B2D5-ENABLED`: ordinary plus initial under enabled serial,
  G0/G1/G3 PASS.

Remaining B3b2d Rebuild call sites, each independently gated:

- B3b2d6: in-loop ghost rebuild after AMR/partition in
  `src/simulation/simulation.h`.
- B3b2d7: `AMRController::execute_amr` refine rebuild in
  `src/amr/amr_controller.h`.

Any recurrence of the Sedov drift immediately re-blocks the route; do not
rerun an unchanged failed state as evidence.

**Submilestone B3c:** freeze the budget from measured high-water results and
remove the temporary probe or retain it behind a disabled diagnostic switch;
G0/G1/G3.

**Hard limit:** proposed `sizeof(quad_data_t) <= 8192` bytes.  Exceeding it
requires explicit review and a split payload plan, not an automatic exception.

**Exit:** `docs/nodal-refactor-b3-memory-budget.md` includes measured dynamic
high-water evidence, not only final VTU cell counts; all three submilestones
PASS.

### T1 - Scalar POD storage types

**Objective:** add new types without embedding or calling them.

**Allowed files:** new `src/nodal/nodal_storage.h`, focused compile test under
`python/`, and build dependency metadata if required.

**Types:** fixed-underlying enums for role/phase/validity; scalar `Vec2Storage`
and `Mat2Storage`; `StageStamp`; `EdgeSegmentGeometry`; `FaceData`;
`CellMasterContribution`; `CellHangingContribution`;
`AggregatedHangingContribution`; `CondensedMasterContribution`;
`MasterSolveState`; `EvaluatedCellFlux`; and `CellNodalData`.

`CellHangingContribution` (`M_ch,b_ch`), stack-only
`AggregatedHangingContribution` (`M_h,b_h`), and endpoint-ledger
`CondensedMasterContribution` (`omega*M_h,omega*b_h`) must be three distinct,
non-convertible types.  Use `uint8_t`, not `bool`, for wire-visible flags.

**Forbidden:** include from `defines.h`, add to `quad_data_t`, or add a runtime
writer/reader.

**Submilestone T1a:** establish the micro-gate runner, C++ compile helper,
machine-readable summary, one passing POD fixture, and one intentionally
non-trivial/pointer-bearing rejected fixture.  Close with G0/G1/G3.

**Submilestone T1b:** add the storage types.  Run `MG-LAYOUT`, `MG-RESET`;
assert size budget and raw-byte round trip.  Then G0/G1/G3.

### T2 - Pure topology and orientation mapping

**Objective:** freeze all corner/face/segment mapping before callbacks use it.

**Allowed files:** new `src/nodal/topology_mapping.h` and focused tests.

**Actions:** implement mapping for four local faces/corners, face endpoints,
fine sibling order, segment endpoint roles, reverse normals, p4est orientation,
`LeafCellKey`, connectivity-canonical `CanonicalNodeKey`, and
`CanonicalHangingFaceKey`.  Return an explicit error for invalid relationships.
Keys are pure diagnostic values; they do not introduce a global production
node/face structure.

Tests must explicitly bridge the three existing enum orders: quad corners
`LB,LU,RU,RB`; p4est corners `LB,RB,LU,RU`; and both edge orders
`LEFT,RIGHT,BOTTOM,UP` and `LEFT,UP,RIGHT,BOTTOM`.  Add a real multi-tree
p4est connectivity integration fixture; pure array tests are insufficient for
cross-tree orientation.  It must prove incident leaves at the same physical
cross-tree master produce the same node key, different nodes produce different
keys, and keys are independent of rank and iteration order.

**Forbidden:** comparing cells from different trees only by `qx/qy`; using
`quadid` as a stable key; callback edits.

**Submilestone T2a:** add complete topology fixtures plus wrong orientation,
invalid endpoint, and cross-tree coordinate negative cases; prove the runner
rejects them; G0/G1/G3.

**Submilestone T2b:** add mapping implementation; run `MG-TOPO`, then
G0/G1/G3.

### T3 - Pure segment geometry

**Objective:** implement regular/split face geometry independent of p4est
storage and solver algebra.

**Actions:** construct one-segment conforming faces and two-segment hanging
faces with per-segment normal, length, and two endpoint metric weights.  Add
scalar conversion helpers only at kernel boundaries.

**Forbidden:** master assembly or cylinder solver activation.

**Submilestone T3a:** add analytic regular/split geometry fixtures and broken
normal/length/metric-weight negative fixtures; close with G0/G1/G3.

**Submilestone T3b:** add implementation; run `MG-GEOM-P` and pure-geometry
`MG-GEOM-C`; then G0/G1/G3.

### L1 - Embed inert CellNodalData

**Objective:** append one inert shadow object to every leaf payload.

**Submilestone L1a:** as one indivisible raw-ABI safety change, include T1
storage, append one member to `quad_data_t`, and deterministically reset it in
the initial forest callback before any exchange, snapshot, or read.  Embedding
uninitialized wire bytes as a separately passing anchor is forbidden.  Verify
actual size, poison/reset, refresh-idempotence compatibility, and G0/G1/G3.

**Submilestone L1b:** reset only refine-created children; G0/G1/G3.

**Submilestone L1c:** reset only coarsen-created parent; G0/G1/G3.

**Submilestone L1d:** reset/invalidate after balance for all local leaves;
G0/G1/G3.

**Submilestone L1e:** reset/invalidate local leaves after partition; no ghost
allocation change; G0/G1/G3.

**Submilestone L1f:** reset ghost destination storage during ghost allocation,
then verify exchange raw-byte round trip and current stamps; measure actual
ghost exchange; G0/G1/G3.

**Authority:** legacy remains the only reader and writer of physical state.

**Forbidden:** populate physics fields or add any production reader.

**Micro-gates:** the event-specific `MG-LAYOUT`, `MG-RESET`, raw ghost round
trip, and memory ceiling.  Do not combine lifecycle boundaries.

### L2 - Boundary mirror

**Objective:** mirror legacy boundary classification into the new layout.

**Actions:** from the same canonical boundary input, populate fixed-capacity
new boundary records; compare type, value, normal, length, and deterministic
constraint order against legacy.

**Forbidden:** independently infer a different boundary or switch a reader.

**Exit:** shadow equality for local and ghost leaves; G0/G1/G3.

### L3 - Regular planar geometry mirror

**Objective:** independently construct conforming one-segment `FaceData`.

**Actions:** one callback writer only; compare new and legacy `Ncp/Lcp/Rcp`,
endpoint identities, and cell closure without changing consumers.

**Micro-gate:** `MG-GEOM-P`; then G0/G1/G3.

### L4 - Hanging geometry mirror

**Objective:** represent every nonconforming face as two physical segments and
a virtual hanging corner on the coarse leaf.

**Actions:** handle all four directions, both fine-side orders, orientation,
shared fine/coarse normal-length relations, and endpoint weights.

**Forbidden:** assemble `M_h,b_h`, inject master contributions, or persist an
aggregate physical flux.

**Micro-gates:** `MG-TOPO`, `MG-GEOM-P`, pure `MG-GEOM-C`; then G0/G1/G3.

### L5 - Epoch and invalidation lifecycle

**Objective:** make stale transient nodal data impossible to consume.

**Submilestone L5-0:** create `MG-EPOCH/MG-AMR` infrastructure with stale-read
negative fixtures for each lifecycle event; prove rejection; G0/G1/G3.

**Submilestones:** L5a initial/stage reset; L5b geometry epoch; L5c refine;
L5d coarsen; L5e balance; L5f partition; L5g ghost rebuild.  Each submilestone
has one writer and a complete G0/G1/G3 closure.

**Micro-gates:** `MG-RESET`, `MG-EPOCH`, `MG-AMR` event matching the substep.

### L6 - One-accessor-at-a-time shadow reads

**Objective:** prove DBGF accessors can read the new layout without changing
the production authority or legacy numerical semantics.

**Submilestones:** L6a boundary accessor audit; L6b regular-matrix accessor
audit; L6c regular-force accessor audit; L6d parent/hanging-geometry accessor
audit.  Each accessor is called only from shadow verification code.  Compare
its result against the current production reader and run G0/G1/G3 after each.

**Forbidden:** production reader switching, DBGF math, old field deletion, or
multiple accessor groups.  Production readers switch only as part of the
complete E1 DBGF pipeline.

### A0 - Candidate acceptance contract

**Objective:** freeze all scientific and numerical acceptance criteria before
the first DBGF candidate result is observed.

**Actions:** define scale-aware thresholds and required fixtures for local
identities, eigenvalue/condition limits, master residual, fully discrete GCL,
momentum, energy, boundary work, positivity, rank equivalence, convergence,
and runtime/memory regression.  Record formulas, units, scaling, aggregation,
and failure policy in a versioned acceptance document.

The solve policy must distinguish a genuinely unconstrained singular master
system from a valid wall/symmetry-constrained reduced-dimensional solve.
Freeze the boundary reduction, rank test, condition metric, and residual norm
before candidate results; a single full-matrix condition threshold must not
reject valid constrained nodes.

**Forbidden:** choosing or loosening a threshold after inspecting S/E results.

**Exit:** independent review of the contract; G0/G1/G3.

### S1 - Pure DBGF local algebra

**Objective:** implement equations from sections 3, 5, 8, and 15 without
p4est callbacks.

**Actions:** direct/hanging `M,b`, branch pressures, branch forces, physical
force, conjugate power, `D_ch`, and scale-aware residuals.

**Submilestone S1a:** add analytic algebra fixtures, permutation cases, and
deliberately broken pressure/force/dissipation kernels; prove rejection;
G0/G1/G3.

**Submilestone S1b:** implement the pure kernels.  Run `MG-LOCAL`, `MG-FORCE`,
degenerate and near-singular inputs; then G0/G1/G3.

### S2 - Cell-local shadow contributions

**Objective:** compute new direct, fine-hanging, and coarse-virtual local
contributions on each cell owner.

**Submilestone S2a:** add one volume writer for cell-local shadow contributions;
no exchange or remote reader; G0/G1/G3.

**Submilestone S2b:** add local oracle/identity comparisons below; no callback
order change; G0/G1/G3.

**Submilestone S2c:** after all local writers finish, add the explicit
`session.exchange()` publication boundary and stamp validation; prove remote
fine values are current before S3 can read them; G0/G1/G3.

**Checks:** compare only regular contributions that are
mathematically identical to legacy `idcn/ide` values.  Fine-hanging and
coarse-virtual `M_ch,b_ch` are new formulas and must be checked against an
independent hand-calculated oracle plus PSD and `r_pi/r_F/r_D` identities.
Never change DBGF hanging math merely to reproduce a legacy value.

**Forbidden:** face aggregation, master solve, or production updates.

**Exit:** finite/stamped local and ghost values; `MG-LOCAL`; G0/G1/G3.

### S3 - Unique coarse-owner hanging aggregation

**Objective:** compute each physical hanging `M_h,b_h` exactly once.

**Submilestone S3a:** create forced local/cross-rank face-ledger fixtures,
including missing-owner, duplicate-owner, and ghost-write negative cases;
prove `MG-MPI` detects them; G0/G1/G3.

**Submilestone S3b:** implement the aggregation callback below.

**Actions:** after publishing S2 locals, run a face traversal.  Only the local
coarse-cell owner may aggregate; ghost records are read-only.  Keep `M_h,b_h`
on the stack as `AggregatedHangingContribution` and emit an audit ledger keyed
by `CanonicalHangingFaceKey`.  The ledger lives only in a verification context;
it is neither per-leaf persistent aggregate storage nor global production
authority.  Every remote read must reject a mismatched `StageStamp`.

**Forbidden:** persistent aggregate payload or endpoint injection.

**Micro-gates:** unique owner, no ghost writes, no missing/duplicate faces,
1/2/4-rank count equality.  Then G0/G1/G3.

### S4 - Endpoint condensation and exchange

**Objective:** publish two weighted endpoint contributions per hanging face.

**Submilestone S4a:** create analytic condensation fixtures with assignment
instead of `+=`, wrong weight, missing endpoint, duplicate endpoint, and
missing-exchange negative cases; prove `MG-CONDENSE` rejects them; G0/G1/G3.

**Submilestone S4b:** implement only the coarse-owner endpoint condensation
writer below; do not add/change an exchange; G0/G1/G3.

**Submilestone S4c:** add the explicit condensed-contribution publication
exchange and stamp validation as its own phase boundary; G0/G1/G3.

**Actions:** coarse owner performs `endpoint.condensed += omega*aggregate` for
both endpoints; use accumulation, never assignment.  Complete the face
traversal, then exchange, then start a separate corner traversal.

**Micro-gates:** `MG-CONDENSE`, `MG-MPI`; assert global endpoint commits equal
`2*num_hanging_faces`.  Run G0/G1/G3, with G3 twice.

### S5 - Shadow master assembly and solve

**Objective:** solve the enhanced master system without writing production
velocity.

**Submilestone S5a:** create analytic well-conditioned, scaled, singular, and
near-singular solve fixtures; prove `MG-SOLVE` rejects nonfinite/singular/high-
residual cases; G0/G1/G3.

**Submilestone S5b:** implement only shadow master matrix/RHS assembly; do not
solve or publish velocity; G0/G1/G3.

**Submilestone S5c:** implement only the local shadow 2x2 solve from S5b
assembly; do not add/change exchange; run `MG-SOLVE` and G0/G1/G3.

**Submilestone S5d:** after all local master solves, add an explicit shadow
master-velocity exchange and stamp validation.  S6 must not recover hanging
state from an unexchanged or stale master velocity.  G0/G1/G3.

**Actions:** corner traversal accumulates direct local plus condensed hanging
contributions; solve one local 2x2 system and write only shadow solve state.
All local leaf copies of the same master must agree after stable-key alignment.
Alignment uses `CanonicalNodeKey`, not leaf-corner tuples or `quadid`.

**Micro-gate:** `MG-SOLVE`; no-hanging result equals regular legacy solve.
Then G0/G1/G3.

### S6 - Shadow hanging recovery, force, and power

**Objective:** recover `U_h`, branch force, physical cell force, and conjugate
power using local `M_ch,b_ch` and the two master velocities.

**Submilestone S6a:** recover and verify shadow `U_h` only; G0/G1/G3.

**Submilestone S6b:** compute and verify shadow branch pressures/forces only;
G0/G1/G3.

**Submilestone S6c:** combine and verify shadow physical cell force only;
G0/G1/G3.

**Submilestone S6d:** compute and verify conjugate branch power and `D_ch`
only; run `MG-FORCE`; G0/G1/G3.

**Actions:** execute recovery on every rank; each rank writes local cells only.

**Forbidden:** writing production momentum/work/energy or using aggregated
force as a cell force.

**Micro-gates:** `MG-FORCE`, pressure split, force recovery, `D_ch`; then
G0/G1/G3.

### S7 - Complete shadow time-step audit

**Objective:** run every DBGF phase in shadow while legacy remains authoritative.

**Submilestone S7a:** create `MG-GCL/MG-CONS` infrastructure with analytic
swept-volume, boundary impulse/work, broken-sign, missing-face, and duplicated-
work negative fixtures; G0/G1/G3.

**Submilestone S7b:** construct candidate next-corner coordinates and volumes
in a shadow-only buffer, never production state.  This is required for a real
fully discrete swept-volume GCL check; a half-discrete identity is not enough.
Close with G0/G1/G3.

**Submilestone S7c:** integrate the complete shadow audit below.

**Actions:** add shadow residual reports for master, GCL, momentum, energy,
boundary work, epoch validity, owner ledgers, and finite/positive state.

**Exit:** legacy final VTU remains unchanged; all DBGF diagnostics pass on
serial and 1/2/4 ranks.  Run `MG-GCL`, `MG-CONS`, `MG-MPI`, `MG-AMR`, then
G0/G1/G3.

### E1 - Whole-pipeline experimental selector

**Objective:** make complete DBGF independently runnable while default remains
legacy.

**Submilestone E1a:** add `Legacy` and `DBGFExperimental` selector type,
configuration validation, step-start freeze, summary metadata, and
`MG-FALLBACK`; default remains legacy and DBGF selection aborts as unavailable.
Update the current golden runners to request `Legacy` explicitly and restore
the parameter file byte-for-byte, so a later default change cannot make them
compare DBGF output with legacy assets.  G0/G1/G3.

**Submilestone E1b:** add a complete DBGF orchestration function, but keep it
unreachable from the runtime selector.  It sequences reset, local assembly,
exchanges, condensation, master solve, hanging recovery, divergence/volume,
candidate coordinate update, momentum, work, energy, and downstream nodal
consumers.  G0/G1/G3.

**Submilestone E1c:** wire every DBGF nodal consumer into that still-unreachable
function and prove its candidate state matches the already validated S7
shadow pipeline.  DBGF excludes old `FluxRelaxed` throughout.  A function
that only switches Riemann/force/power is incomplete.  G0/G1/G3.

**Submilestone E1d:** change one availability/selector condition so the entire
pipeline becomes atomically reachable in `DBGFExperimental`.  No inner
callback selector or partial activation is allowed.  Run `MG-FALLBACK`, the
candidate suite, and legacy G0/G1/G3.

**Forbidden:** callback-level fallback, mixed mode, or activating a partial
DBGF pipeline.

**Micro-gate:** `MG-FALLBACK`; legacy G0/G1/G3 must remain unchanged.

### E2 - No-hanging degeneration

**Objective:** prove DBGF reduces to the regular solver on uniform conforming
meshes.

**Actions:** uniform translation/compression and Noh uniform candidate runs.

**Exit:** topology and selected state agree with regular solver at frozen
tolerances; legacy G0/G1/G3 plus candidate no-hanging gate.

### E3 - Static serial hanging patch

**Objective:** verify one and multiple hanging faces without MPI ambiguity.

**Fixtures:** one hanging face; one master adjacent to multiple hanging faces;
same coarse corner adjacent to two hanging faces; physical-boundary hanging.

**Gates:** topology, ledger, master residual, GCL, momentum, energy, boundary
work, positivity; then legacy G0/G1/G3.  Do not update goldens.

### E4 - Static 1/2/4-rank hanging patch

**Objective:** prove owner/exchange semantics across a forced partition edge.

**Actions:** same stable CellKey fixture at 1/2/4 ranks; assert at least one
cross-rank hanging face and rank-independent global topology/ledger/residuals.

**Gates:** `MG-MPI`, `MG-CONDENSE`, `MG-CONS`; legacy G0/G1/G3 twice.

### E5 - Dynamic AMR lifecycle

Each event is a separate submilestone and commit candidate:

- E5a refine-only;
- E5b coarsen-only;
- E5c balance-induced refine only;
- E5d partition-only owner migration;
- E5e full dynamic AMR after the four isolated events pass.

For every event verify reset/epoch, geometry rebuild, ghost publication,
stable topology, no stale cache, GCL, conservation, and 1/2/4-rank behavior.
Run event-specific `MG-AMR`, then full G0/G1/G3.

### E6 - Planar DBGF physical certification

**Objective:** assemble the scientific evidence required before a new golden.

**Cases:** Noh/Sod/Sedov, smooth acoustic wave and vortex crossing a coarse-
fine interface, Galilean-translated equivalents, uniform translation,
compression/expansion, linear
velocity GCL, strong expansion/near-vacuum, static/dynamic AMR, and a mesh
refinement convergence study.  Include `U_a=U_b => D_ch=0` and separate
normal/tangential master-relative-velocity cases.  A small strict constrained-
virtual-work coupled oracle is mandatory; omission requires an explicit user
waiver recorded with the acceptance evidence.

**Exit:** frozen conservation/GCL/positivity/convergence criteria pass and an
intentional legacy-vs-DBGF difference report exists.  No reference changes.

### A1 - Candidate freeze and explicit approval

**Objective:** freeze one DBGF formula/configuration and request scientific
approval.

**Deliverable:** commit, solver mode, formulas/version, parameters, micro-gate
results, 1/2/4-rank evidence, conservation errors, convergence evidence, and
intentional legacy differences.

**Hard stop / approval 1:** without explicit user approval to generate
versioned DBGF assets, remain experimental and do not modify `reference/`.

### A2 - Versioned DBGF golden creation

**Precondition:** explicit approval after A1.

**Submilestone A2a:** add an isolated generation tool that writes only below a
unique `.tmp/dbgf-candidate-*` directory; dry-run it and review metadata,
pieces, fields, topology, and conservation report; G0/G1-L/G3-L plus candidate
micro-suite.

**Submilestone A2b:** after approval 1, generate and add immutable candidate
assets in `reference/dbgf-v1/`; do not add or change runners in the same diff;
verify the legacy reference manifest is unchanged; run G0/G1-L/G3-L.

The DBGF manifest must include formula version, solver mode, full parameter
hash/content, terminal step and time, rank count, expected filenames, complete
piece list, and per-file hashes.

**Submilestone A2c:** add permanent explicit `run_legacy_tests.py`,
`run_legacy_mpi_gates.py`, `run_dbgf_tests.py`, and
`run_dbgf_mpi_gates.py`, summary schemas, solver-mode matching, and SOP.  Keep
canonical `run_tests.py/run_mpi_gates.py` on legacy until A3b.  Run G0,
G1-L/G3-L, and G1-D/G3-D.  DBGF runners must select terminal output from
the versioned manifest or another independently verified terminal rule; they
must not reuse legacy hard-coded steps `3046/3933`.  Generation and validation
use isolated output directories and reject stale or unexpected pieces.

**Forbidden:** overwriting, moving, or renaming legacy reference files;
changing tolerance; combining reference generation with default switch.

**Exit:** new G1-D/G3-D runners verify binary mode matches golden mode.

### A3 - Default switch

**Precondition:** A2 assets and runners independently reviewed.

**Hard stop / approval 2:** obtain explicit user approval to change the
executable default.  Approval 1 to generate assets does not authorize this.

**Submilestone A3a:** switch only the executable's top-level default selector;
keep canonical SOP/aliases on legacy; run both explicit rails plus default-
mode verification three times.

**Submilestone A3b:** separately switch canonical gate aliases/SOP to DBGF and
require runner/binary/reference mode metadata agreement; keep explicit legacy
rail available during stabilization.

**Exit:** G0 and complete G1-D/G3-D pass three consecutive times; selector
metadata appears in summaries; legacy compatibility gates still pass if the
legacy mode remains supported.

### D1 - Stop legacy writes

**Objective:** make DBGF the sole production authority before deleting fields.

**Hard stop / approval 3:** obtain explicit user approval to retire legacy
runtime compatibility.  Default-switch approval does not authorize removal.

**Actions:** stop one legacy writer group at a time; poison retired fields in
debug verification mode; prove production zero-read with whole-repository
search and runtime read guards.

**Forbidden:** field deletion or layout compression.

**Exit:** each writer group separately passes G0/G1-D/G3-D.

### D2 - Remove FluxRelaxed path

**Order:** remove/disable production consumers; prove no reads; remove reset
and producer; remove `idcnFluxRelaxed`; remove
`ParentBounInfo::FluxRelaxed`.  Each item is a separate submilestone.

**Forbidden:** removing fields before zero-reader proof or adding `D_ch` a
second time to DBGF energy.

**Exit:** every submilestone passes G0/G1-D/G3-D.

### D3 - Remove legacy point/parent/half-edge fields

**Order:** dead flags; old point aggregate/velocity fields; old parent physics;
legacy edge fields; half-edge storage; obsolete `CVariable ide*` slots; then
the container types themselves.  `CPointBounInfo`, `ParentBounInfo`, and
`CHalf_edge_data` are removed only after their last reader and function
signature are gone.

Each field group requires zero-read/write search evidence, poison evidence,
size measurement, and G0/G1-D/G3-D.  Do not delete the structures in one diff.

### D4 - Final layout and lifetime closure

**Objective:** remove double-layout overhead and close the whole-record
lifecycle debt.

**Submilestone D4a:** compact DBGF storage only after profiling; no lifetime or
numerical changes; traits/size tests and G0/G1-D/G3-D.

**Submilestone D4b:** establish a formal C++14 whole-record lifetime strategy.
For every allocation/transfer/destruction entry, either provide a defensible
C++14 object-lifetime proof for the exact storage operations or introduce an
explicit byte-storage adapter with placement-new/destruction/lifetime
wrappers.  Do not apply C++20 implicit-lifetime rules retroactively.  Cover
initial allocation, refine, coarsen, balance, partition, ghost allocation/
exchange, and destruction.  Trait/static assertions alone are not object-
lifetime proof.  G0/G1-D/G3-D.

**Submilestone D4c:** remeasure dynamic local/ghost high-water, MPI bytes,
memory, and runtime; compare with B3/A0 ceilings; two consecutive G3-D.

**Exit:** final traits/size audit, no legacy solver reads, full micro-suite,
G0/G1-D, and two consecutive G3-D passes.

### C0 - Cylindrical DBGF separate project

**Precondition:** planar D4 complete.

**Objective:** close the planar program boundary without implying cylindrical
support.  Current behavior is a silent partial run: configuration accepts
`coord_type=1`, while `SolverGate::should_run_riemann(Cylinder)` skips Riemann
and later updates can continue.  This is not fail-closed rejection.

**Submilestone C0a:** audit and test the current silent-skip path; document all
stages that still run for cylinder; no behavior change; G0/G1-D/G3-D.

**Submilestone C0b:** add startup/mode validation that reports cylinder as
unsupported and exits nonzero before simulation state advances.  Add positive
planar and negative cylinder solver-gate tests.  Do not activate cylindrical
physics.  G0/G1-D/G3-D.

**Submilestone C0c:** prove T3 only tested pure geometry and produce a separate
cylindrical taskbook covering volume, segment weights, GCL, Riemann physics,
boundary work, positivity, MPI/AMR, and separate goldens; request independent
scope approval; G0/G1-D/G3-D.

**Forbidden:** enabling cylinder merely because T3 geometry tests pass or
reusing planar goldens.

**Exit:** the separate cylindrical project is scoped and explicitly approved,
deferred, or rejected.  C0 PASS does not mean cylinder is implemented.

## 7. Completion checklist

The refactor is complete only when all statements are evidenced:

- every B0-C0 program milestone (including A0-A3) and declared submilestone
  has a PASS record;
- per-leaf DBGF storage is the sole production authority;
- no DBGF production path reads or writes legacy `FluxRelaxed`, parent-extra,
  point aggregate, half-edge, or obsolete `ide*` storage;
- local, condensed, and evaluated quantities remain type-distinct;
- owner/ghost and exchange contracts pass 1/2/4-rank fixtures;
- refine/coarsen/balance/partition cannot consume stale stage data;
- master residual, fully discrete GCL, momentum, energy, boundary work, and
  positivity gates pass;
- versioned goldens match the selected solver mode and legacy assets remain
  intact;
- final whole-record size/lifetime audit and continuous gates pass;
- C0 proves cylindrical support is still gated and hands off a separately
  approved/deferred project; planar completion never implies cylinder support.

Do not report completion from a clean search alone.  Inspect the final code,
gate summaries, reference manifests, runtime mode metadata, and milestone
records requirement by requirement.

## 8. Current handoff state (2026-08-23)

The current branch is `main` at
`9c280bd35fc45eb6edcdc61a11474e571c8b67a8`, with pre-existing user document
changes that must be preserved.

| Milestone | Current evidence | Handoff decision |
|---|---|---|
| B0 | G0/G1 PASS once and complete G3 PASS three times on current main | useful evidence, but the strengthened taskbook requires the full G0/G1/G3 sequence three times; recertify B0 |
| B1 | contract document present; G0/G1/G3 observed PASS | preserve document; create durable milestone record before next code change |
| B2 | lifecycle audit present; G0/G1/G3 observed PASS | preserve corrected audit; create durable milestone record |
| B3 | size projection present; G0/G1 PASS, but G3 was interrupted | **NOT CLOSED**; after recertifying B0 and recording B1/B2, restart B3a/B3b/B3c with dynamic high-water measurement |
| T1 onward | not started | follow this taskbook in order |

The interrupted B3 G3 left `param.ini` temporarily changed.  It has been
restored and verified:

```text
param.ini SHA-256:
C7C9FCB62D02E3FA8287B00390068DC75D27D9061ED2BC6BC5ED05324F56B83D

reference files: 17
reference manifest SHA-256:
1CAB380700FF49C8BB16DA12464463CB699DCC7028F624953AEAE4D1B5F05EF6
```

No `AMR_Solver` or `mpiexec` process was running at handoff.  The next C++
agent starts by recertifying B0 under this taskbook, then persists B1/B2 gate
records before B3a.  It must not treat the interrupted G3 or the current
provisional B3 estimate as a PASS anchor.

## 9. Independent review record

One independent agent reviewed this taskbook against the DBGF mathematics,
current p4est implementation, and golden-gate SOP.  Its blocking findings were
incorporated into the work orders:

- named micro-gates now require executable positive and negative fixtures;
- G1 protects and restores the entire pre-existing `output/`, while G3 rejects
  stale pieces and validates modification times;
- B3 now measures dynamic local/ghost/exchange high-water instead of inferring
  peak memory from final VTU counts;
- L6 remains shadow-only until the whole DBGF selector is atomically enabled;
- local, aggregate, condensed, and evaluated quantities have distinct types;
- every remote-read phase has an explicit exchange and `StageStamp` check;
- cross-tree node/face diagnostics use connectivity-canonical keys;
- legacy/DBGF rails and three user approvals are separate;
- C++14 lifetime closure cannot rely on C++20 implicit-lifetime rules;
- C0 converts the current cylinder silent-skip into fail-closed unsupported
  behavior without activating cylindrical physics.

The reviewer found no remaining blocking issue after these changes.  Its last
numerical qualification, boundary-aware singular/rank handling in
`MG-SOLVE`, is frozen in A0 before candidate results are observed.
