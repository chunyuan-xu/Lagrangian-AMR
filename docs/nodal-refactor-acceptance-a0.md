# A0: Candidate Acceptance Contract (frozen)

Version: 1.0
Status: proposed/frozen before S/E candidate results.

This contract fixes the numerical and scientific acceptance criteria for the
DBGF nodal solver candidate before any candidate result is inspected.
Thresholds below are scale-aware and must not be loosened after observing S/E
results.

## 1. Common scale and tolerance policy

For a quantity with characteristic scale `S`, an absolute acceptance tolerance
is

    eps_abs(S) = C * eps_machine * max(1.0, S)

where `eps_machine = 2.2e-16` for IEEE double and `C` is the per-test constant
listed below.  Relative checks use the same `C` against a denominator that is
bounded away from zero.

| Micro-gate | Scale `S` | `C` | Failure policy |
|---|---|---|---|
| `MG-LOCAL` | max abs entry of `M_ch` | 32 | fail-fast |
| `MG-FORCE` | max abs of branch force | 32 | fail-fast |
| `MG-SOLVE` | max abs of master RHS | 64 | fail-fast |
| `MG-GCL` | swept-volume residual | 32 | fail-fast |
| `MG-CONS` | global momentum / energy | 32 | fail-fast |
| `MG-MPI` | count or ledger sum | 1 (exact integer) | fail-fast |

## 2. Required fixtures

`MG-LOCAL`:
- regular planar cell, positive `Z`, unit normals, equal half-lengths;
- hanging coarse/fine pair with two physical segments;
- deliberately broken pressure/force kernel that must be rejected.

`MG-SOLVE`:
- valid wall/symmetry-constrained reduced-dimensional system;
- genuinely unconstrained singular master that must fail-fast;
- finite-matrix and finite-residual fixtures.

`MG-FORCE`:
- branch pressure, branch force, physical force, conjugate power, and
  `D_ch >= -eps` fixtures with positive and broken kernels.

`MG-GCL`:
- fully discrete swept-volume residual on regular and AMR-refined cells.

`MG-CONS`:
- global momentum minus boundary impulse and energy minus boundary work,
  over serial and 1/2/4-rank MPI runs.

## 3. Boundary solve policy

The solve policy must distinguish a genuinely unconstrained singular master
system from a valid wall/symmetry-constrained reduced-dimensional solve.
Freeze the boundary reduction, rank test, condition metric, and residual norm
here.  A single full-matrix condition threshold must not reject valid
constrained nodes.

## 4. Version control and review

This document is immutable for the S/E candidate route.  Any change requires a
new version, an explicit scientific review, and a new A0 gate before candidate
results are trusted.
