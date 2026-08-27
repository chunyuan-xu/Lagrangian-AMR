# M10L.3 - Strict C++14 Raw-Storage Lifetime Research Gate

## Question

Does the current p4est/legacy model have a strict C++14 object-lifetime proof
for `quad_data_t` across:

- initial p4est allocation;
- AMR refine/coarsen replacement;
- balance and partition movement;
- ghost allocation and byte exchange;
- destruction?

Trivial-copyability traits alone are not a lifetime proof.

## Current Evidence

### Allocation

`main.cpp` calls `p4est_new_ext(..., sizeof(quad_data_t),
Initializer::Lagrangian_init_condition, ...)`. p4est allocates raw bytes and
does not call a `quad_data_t` constructor. The init callback writes selected
fields but not every byte or padding byte.

### AMR replacement

`AMRCallbacks::Lagrangian_replace_quads` receives raw `quad_data_t*` records
and writes child/parent records through raw pointers. No placement-new or
destructor call occurs.

### Balance and partition

`AMRController::execute_amr` calls `p4est_balance_ext` with the replace
callback. `execute_partition` calls `p4est_partition`. Both move raw payload
bytes without C++ lifetime operations.

### Ghost buffer

`GhostSession::initialize` allocates raw `quad_data_t` with `P4EST_ALLOC` and
calls `p4est_ghost_exchange_data`, which copies raw bytes. No construction or
destruction runs per record.

### Destruction

`p4est_destroy` and `GhostSession::destroy` free raw storage without calling
nested destructors. No owned resources exist in the current record.

## Decision

The C/p4est model cannot be closed as a strict C++14 lifetime proof without a
broader adapter that covers every route above. This package **defers** that
proof explicitly. No partial placement-new, destruction loop, or object-lifetime
adapter is added in this package.

The deferred boundary remains recorded in the M10L.0 ledger and in
`docs/nodal-refactor-b2-lifecycle-audit.md`.

## Gate Closure

This is an audit-only package with no production source diff. It reuses the
last clean G0/G1/G3 from the M10L.2 closure at commit `4adf603`, because the
production tree is byte-identical since that closure.

```text
package: M10L.3
base commit: 4adf603
focused verification: lifetime evidence recorded; deferral explicit
G0: reused from M10L.2 clean build PASS
G1: reused from M10L.2 serial golden PASS
G3: reused from M10L.2 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
