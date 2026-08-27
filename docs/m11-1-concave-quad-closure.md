# M11.1 - Concave-Quadrilateral Predicate Package Closure

## Change

Added a compiler-visible fail-closed return path to
`GeometryAlg::is_concave_quad` in `src/alg.cpp`. All reachable paths already
returned; the new default returns `-1` (not concave), matching the first
branch's convention.

## Fixtures

Added `python/test_m11_1_concave_quad.py`, which compiles against `alg.cpp`
and asserts:

- convex unit square returns `-1`;
- concave quad returns a concave vertex index `>= 0`;
- collinear degenerate quad returns `-1` (not concave).

## Degenerate Caller Expectations

The predicate is used by AMR/hydro children-info geometry. The current policy
treats collinear/zero-area shapes as not concave. Changing that policy requires
a new package.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m11_1_concave_quad.py --summary .tmp/mg-m11-1-concave-quad.json
MG-M11-1 PASS
```

## Gate Closure

```text
package: M11.1
base commit: cc27973
focused verification: MG-M11-1 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
