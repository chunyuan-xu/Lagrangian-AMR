# Legacy Solver Optimization and Refactoring Taskbook

## 1. Scope and Constraints

This taskbook refactors and measures the legacy solver only. DBGF remains
deferred to a later version.

- Canonical authority remains in fixed-size per-leaf `quad_data_t` storage.
- `p4est_data_t` remains global bridge state, not leaf payload.
- `p4est_partition` always migrates complete canonical `quad_data_t` records.
  A compact ghost record, if later justified, is a separate read-only wire view;
  it is never cast to `quad_data_t*` and never replaces partition migration.
- Do not modify `src/nodal/**`, DBGF tests, `reference/**`, golden tolerances,
  or golden field selection.
- Do not create external authoritative arrays, owning pointers, vectors, or
  variable-size leaf payloads. Member reordering and field deletion require an
  explicit later package.

## 2. Starting Point

- Branch: `main`; accepted implementation anchor: `ed78788`.
- The legacy baseline is accepted. Do not rerun a pre-change baseline merely to
  reopen the work.
- `context.md` is an untracked user handoff file and is outside commit scope.
- M9 main extraction and M10 `P4estBridge` are historical closures.

## 3. Gate-Package Model

The prior taskbook contained 242 editing-level actions. This replacement has
**56 explicitly named non-template gate packages**, plus evidence-triggered
template series. The prior actions are retained as package checklists instead
of triggering a full G0/G1/G3 cycle individually.

A package may combine its audit, focused fixture, pure extraction, and migration
of its unique consumer when they form one observable, reversible behavior slice.
It may not mix numerical semantics, lifecycle/reset, ghost communication, and
payload layout.

### 3.1 Closure Required for Every Activated Package

Every activated package, including audit, fixture, measurement, and production
work, closes at one commit with:

1. focused test, fixture, audit, or measurement for its stated contract;
2. G0 and `git diff --check`;
3. G1 serial golden regression;
4. G3 four-rank golden regression.

An audit-only package can have no production source diff, but it still records
the four passing checks. Conditional `x` packages are dormant until their stated
activation condition is met; each gets its own closure record when activated.

### 3.2 Closure Record

Before editing, record package ID, base commit, hypothesis, allowed files,
read/write/exchange effect, and focused verification. Before the next package:

```text
package:
base commit:
focused verification: command and passing result
G0: summary path and passing status
G1: summary path and passing status
G3: summary path and passing status
param_restored: true
reference hash: unchanged hash
closure commit:
```

### 3.3 Required Local Commit

After the focused check and G0/G1/G3 all pass, each activated package must be
closed by exactly one new **local Git commit** before the next package begins.
Stage only the package's allowed files and its required closure evidence.  Do
not add unrelated user changes, including `context.md`, generated output,
`reference/**`, parameters, or frozen DBGF files.

The commit message starts with the package ID and states its single behavior
slice, for example `refactor(M13.1): extract conforming gradient kernel`.  The
closure record stores the resulting commit hash.  Do not amend an earlier
closed package to add later work; create a new package/commit instead.

`git commit` is required local repository history.  `git push`, GitHub pull
requests, releases, and other remote actions are not implied and require
separate user authorization.

### 3.4 Boundaries That Must Stay Separate

- initial creation, refine, coarsen, balance, and partition reset boundaries;
- owner/ghost exchange movement/removal or a ghost-schema cutover;
- final deletion of one payload field group;
- a numerical kernel with its own formula or failure policy;
- one measured traversal or exchange optimization candidate.

## 4. M10L - Leaf Payload Contracts and Minimal Closure

**Goal:** keep authority in `quad_data_t` while making ownership, definedness,
lifecycle, and remote-read contracts sufficient for safe refactoring.

### M10L.0 - Leaf-Payload Contract Ledger

Produce one evidence ledger for `CVariable`, `CPoint_data_t`,
`CHalf_edge_data`, `CCorner_data`, `ParentBounInfo`, topology, AMR-transfer,
and frozen `nodal` fields. Include current size/offset/traits, allocation/copy/
free routes, writer, first reader, resetter, local/remote authority, and
validity after create/refine/coarsen/balance/partition. For each exchange,
record last local writer -> first remote reader and confirm the current complete
`quad_data_t` wire format. Unknown remains `unknown`, not permission to change.

### M10L.1 - ABI and Definedness Fixture Package

Consolidate read-only layout assertions and add a poison harness for one field
domain. It must reproduce the existing ABI rather than freeze a new layout, and
must fail when the selected required writer is removed.

### M10L.2 - Confirmed Read-Before-Write Repair

Trace, poison-test, and repair the confirmed
`points[].pi_constrained_parent -> ParentPIStar` path. The repair may add the
proven writer or fail-closed guard; it retains storage and does not broaden
reset/transfer behavior.

**M10L.2x:** activate only for an additional M10L.0 evidence-backed defect.
One `x` covers one field domain and first reader.

### M10L.3 - C++14 Raw-Storage Research Gate

Record strict C++14 lifetime evidence for p4est initial allocation, AMR
replacement, balance, partition, ghost allocation/byte exchange, and
destruction. Trivial-copy traits are not a lifetime proof. If the C/p4est model
cannot be closed without a broader adapter, defer it explicitly.

**Hard stop:** do not add partial placement-new or destruction loops; that
would create inconsistent lifetime rules without covering every route.

### M10L.4 - Parent-Edge Reset Contract Package

Define persistent/transient state and inactive-read behavior for one
`ParentBounInfo` slot; add inactive-poison, active-parity, and byte-scope tests;
then introduce `reset_parent_edge_scratch` without wiring it into an event.

### M10L.5 - Initial-Leaf Reset Boundary

Call the M10L.4 helper only during initial leaf creation. Preserve initial
values, active-mask semantics, topology rebuild order, and exchange order.

### M10L.6 - Refine-Child Reset Boundary

Call the helper only for refine-created children, with focused refine coverage.

### M10L.7 - Coarsen-Parent Reset Boundary

Call the helper only for coarsen-created parents, with focused coarsen coverage.

### M10L.8 - Balance Reset Boundary

Call the helper only after balance replacement, preserving existing rebuild,
ghost reconstruction, and publication order.

### M10L.9 - Partition Reset Boundary

Call the helper only after partition migration, or prove that the current route
already preserves the audited transient state. Partition remains full-payload.

### M10L.10 - Demand-Driven Typed View Package

Activate only for one high-risk consumer and one audited domain. Add an
ABI-neutral, non-owning const/mutable view, prove address parity and const-write
rejection, then migrate that consumer. A view covers only one domain:
`CVariable`, points/corners, half-edge, parent-edge, topology, or AMR transfer.

**M10L.10x:** one additional consumer/domain pair. No bulk renaming or view
without a demonstrated consumer risk.

### M10L.11 - Cold-Field Candidate Ledger

For `face_neighbors`, `face_num`, `m_edata`, PI fields, dissipation flags, and
other candidates, collect producer/reader/reset/serialization evidence and a
no-reader probe. Label retain, initialize, deprecate, or delete-later; do not
delete a field here.

## 5. M11 - Correctness and C++14 Portability

### M11.0 - Warning Inventory Package

Capture/classify current `-O2 -g -Wall -std=c++14` warnings, map reachable
correctness warnings to configurations/callbacks, and freeze the count summary.

### M11.1 - Concave-Quadrilateral Predicate Package

Add orientation/collinearity fixtures, define the missing
`GeometryAlg::is_concave_quad` return path, and document degenerate-case caller
expectations. A changed invalid-case policy requires a new package.

### M11.2 - Refinement-Variable Selection Package

Create and fixture one typed `RefineCriteria -> variable IDs` selector; migrate
the four gradient-estimation callbacks and remove their duplicate switches.
Unsupported criteria must fail before variable-array access.

### M11.3 - C++14 Trace-State Package

Replace non-C++14 trace state with a compatible accessor and explicit
`p4est_iterate` callback context; migrate the Riemann counter, remove obsolete
state, and add a multi-translation-unit linkage test.

### M11.4 - Output Formatting and Initialization Package

Correct half-time formatting and sound-speed reporting, define failure behavior,
and initialize or fail closed for distance-profile coordinates with output tests.

### M11.5 - AMR Distance-Guard Package

Add one pure zero-distance guard and migrate conforming, hanging, and corner
gradient uses. Invalid geometry must be reported or rejected, never clamped.

## 6. M12 - Diagnostic and Trace Boundaries

### M12.0 - Diagnostic Inventory and Options Package

Inventory diagnostic producers, flags, extra traversals/MPI work/files; classify
them; introduce immutable startup options; prove disabled mode has no extra
trace traversal, output, or collective.

### M12.1 - Riemann Targeted-Trace Package

Move the step-3 Riemann dump to diagnostics, use stable logical-cell matching,
centralize bounded filename construction, and prove disabled mode returns before
mesh traversal.

### M12.2 - Hydro Edge and Hanging-Trace Package

Move edge-matrix, local-corner, corner-solve, parent-edge, hanging-override,
and hanging-sum traces behind read-only diagnostic records. Numerical callbacks
must not acquire new diagnostic responsibilities.

### M12.3 - AMR Transfer-Trace Package

Isolate refine comparison/parent/child traces, centralize rank-aware file
handling, and prove disabled refine tracing performs no file operation.

## 7. M13 - AMR Callback Decomposition

### M13.0 - AMR Authority Contract Package

Document Reads/Writes/Exchange/Invalidates, owner writes, ghost reads, scratch
writes, forbidden ghost writes, and stable keys for all AMR callbacks.

### M13.1 - Conforming-Gradient Package

Extract/fixture the two-cell conforming gradient calculation, then migrate its
edge-callback branch.

### M13.2 - Hanging-Gradient Package

Extract/fixture the coarse/fine three-cell gradient calculation, then migrate
its edge-callback branch.

### M13.3 - Cell-and-Corner Gradient Reduction Package

Extract/fixture maximum edge-to-cell and corner-neighbor reductions, then
migrate their callbacks without changing exchange boundaries.

### M13.4 - Refine Decision Package

Inventory refine inputs, extract/fixture policy, then migrate refine-error and
default-refine tag callbacks.

### M13.5 - Coarsen Decision Package

Inventory coarsen inputs, extend/fixture policy if needed, then migrate coarsen
error and default-coarsen tag callbacks.

### M13.6 - Refine-Transfer Package

Document parent reads/child writes; extract/migrate geometry and physical
transfer; isolate transient initialization; reduce the callback to event routing.

### M13.7 - Coarsen-Transfer Package

Document child reads/parent writes; extract/migrate geometry and conservative
restriction; isolate transient initialization; reduce the callback to routing.

### M13.8 - Balance Lifecycle Package

Make pre/post-balance validity explicit and isolate existing reset, rebuild,
ghost reconstruction, and publication without moving their order.

### M13.9 - Partition Lifecycle Package

Make owner migration explicit and isolate existing invalidation, ghost rebuild,
and publication without changing full canonical payload migration.

## 8. M14 - Hydro Callback and Phase Decomposition

### M14.0 - Hydro Phase Contract Package

Inventory callback reads/writes, current/half/lagged transitions,
exchange-to-first-reader links, and mixed responsibilities.

### M14.1 - Local Corner-Matrix Package

Fixture/extract local matrix/RHS algebra, migrate its callback, isolate corner
publication, and migrate corner-to-point assembly.

### M14.2 - Regular-Corner Velocity Package

Fixture the legacy 2x2 solve and boundary cases, extract input/result logic,
and migrate owner-local velocity plus lag-to-relaxed copying.

### M14.3 - Hanging Relaxed/Parent-Edge Package

Document, extract, and migrate relaxed-info and parent-edge local algebra only.

### M14.4 - Hanging Aggregation/Solve Package

Document ownership; extract/migrate aggregation and constrained solve; migrate
owner-local writes; isolate existing post-aggregation/post-solve exchanges.

### M14.5 - Divergence Update Package

Fixture/extract the pure divergence kernel and migrate its callback.

### M14.6 - Coordinate Update Package

Fixture/extract the pure coordinate-update kernel and migrate its callback.

### M14.7 - Volume/Density Update Package

Fixture/extract the pure volume/density kernel and migrate its callback.

### M14.8 - Momentum Update Package

Fixture/extract the pure momentum kernel and migrate its callback.

### M14.9 - Corner-Work Update Package

Fixture/extract the pure corner-work kernel and migrate its callback.

### M14.10 - Total-Energy Update Package

Fixture/extract the pure total-energy kernel and migrate its callback.

### M14.11 - EOS and Sound-Speed Package

Fixture/extract EOS and sound-speed kernels and migrate their callbacks. Split
again only if focused evidence reveals an independent numerical discrepancy.

### M14.12 - Controller Phase API Package

Name boundary/half-state, geometry/nodal prerequisites, legacy Riemann, and
conservative-update phases; route `advance_single_stage` through them without
changing phase order.

## 9. M15 - Measurement-Driven Optimization

### M15.0 - Profiling Infrastructure Package

Add default-off timing, rank-local capture, one end-of-run reduction, relevant
counters, bounded output, and proof that disabled profiling adds no traversal or
collective.

### M15.1 - Frozen Performance Baseline Package

Freeze compiler/machine/ranks/parameters/output; measure serial Noh/Sod/Sedov
and 1/2/4-rank Sod/Sedov; report costs and a rejection-aware candidate ledger.

### M15.2x - One Traversal-Fusion Candidate

Activate for one measured candidate only. Prove compatible context and no
intermediate reader/exchange/diagnostic/lifecycle boundary; test equivalence;
fuse; retain only with reproducible benefit.

### M15.3x - One Exchange Optimization Candidate

Activate for one measured exchange only. Prove fresh last-writer/first-reader
ordering, add owner/ghost stamps, move or remove that exchange, then record
1/2/4-rank identity and savings.

### M15.3P - One Phase-Specific Ghost Projection

Activate only when M15.1 proves material communication cost and M10L lists all
remote readers for one phase. Finalize POD schema and fixtures, add independent
custom exchange, and migrate one reader while the full `GhostSession` remains
an oracle. Additional readers use M15.3P-x; cut over only when none remain.

**Hard stop:** unknown reader, compact-buffer `quad_data_t*` cast, or replacing
complete partition migration stops this branch.

### M15.4 - Measured I/O Improvement Package

Measure output costs, then improve only measured producer(s), preserving rank
ownership and flush behavior.

### M15.5 - Memory and Scaling Report Package

Measure local/ghost payload and exchange bytes with DBGF unchanged; compare
1/2/4-rank behavior; record accepted/rejected candidates and remaining costs.

## 10. M16 - Module and Repository Closure

### M16.0 - Obsolete Diagnostic Group Removal

Prove zero callers and remove one obsolete diagnostic helper group.

### M16.1 - Obsolete AMR Wrapper Group Removal

Prove zero callers and remove one obsolete AMR wrapper group.

### M16.2 - Obsolete Hydro Wrapper Group Removal

Prove zero callers and remove one obsolete hydro wrapper group.

### M16.3x - One Leaf-Field Group Removal

Activate only for an M10L.11-approved group. Re-audit writer/reader/reset/
output/ghost/AMR/partition use; remove final writer, reader, and reset while
retaining storage; then delete the group, update ABI evidence, and record actual
1/2/4-rank byte savings. Do not combine with layout reorder, ghost schema,
reset change, or numerical refactoring.

### M16.4 - Main Header-Dependency Cleanup

Inventory direct `main.cpp` includes and remove one proven unused/transitive
batch.

### M16.5 - Public Header-Dependency Cleanup

Make `hydro_controller.h`, `amr_callbacks.h`, and `hydro_callbacks.h`
self-contained; replace legal cycles or oversized dependencies with forward
declarations where appropriate.

### M16.6x - One Translation-Unit Extraction

Activate for one measured/stable non-template implementation group. Add its
declaration header, `.cpp` implementation, Makefile object, caller switch, and
old implementation removal after compile/link evidence.

### M16.7 - Final Contracts and Handoff Package

Update architecture/phase graphs and AMR/hydro contract tables; record final
warnings, C++14 exceptions, serial/MPI performance/memory, repository hygiene,
and the deferred DBGF boundary.

## 11. Execution Order and Stop Rules

```text
M10L leaf payload contracts and minimal closure
  -> M11 correctness and portability
  -> M12 diagnostics
  -> M13 AMR decomposition
  -> M14 hydro decomposition
  -> M15 measured optimization
  -> M16 repository closure
```

Stop a package when focused verification or G0/G1/G3 fails; ownership cannot be
proven; an optimization lacks benefit; it requires DBGF/`src/nodal/` work; or it
crosses a non-mergeable boundary. Never loosen tolerance, regenerate reference,
or combine changes to manufacture closure.

## 12. Definition of Done

- legacy numerical behavior remains authoritative and reproducible;
- every activated package has focused evidence and G0/G1/G3 closure;
- AMR/hydro callbacks are thin adapters over testable logic;
- owner/ghost, exchange, reset, and invalidation contracts are explicit;
- retained optimizations show reproducible benefit;
- `quad_data_t` remains fixed canonical leaf payload;
- `reference/` and DBGF implementation remain unchanged.
