# M11.0 - Warning Inventory

## Capture

Clean build with the formal G0 toolchain flags `-O2 -g -Wall -std=c++14`:

```text
make clean && make -j8
BUILD_EXIT=0
```

Captured log: `.tmp/m11-0-warnings.log`

## Frozen Count Summary

| Metric | Count |
|---|---:|
| total warning lines | 100 |
| error lines | 0 |
| build exit | 0 |

## Classification

### Correctness-relevant

- `control reaches end of non-void function` in
  `GeometryAlg::is_concave_quad` (`src/alg.cpp`): missing return path.
- `may be used uninitialized` for `idCPara`, `idEPara`, `idCNPara`, and
  `distance`: mapped to AMR/hydro minmod and distance-profile callbacks.
- `too many arguments for format` in half-time illegal-state messages
  (`hydro_callbacks.h`): extra `P4EST_DIM` argument.
- C++17 `inline variables` (`core/trace.h`, nodal mirror headers): portability
  debt under `-std=c++14`.

### Non-correctness

- unused variables and set-but-not-used variables (`50` unused variable lines
  plus related set-but-not-used entries).
- unused parameters and local pointers/casts.

## Reachability Mapping

| Warning family | Configuration/Callback |
|---|---|
| `is_concave_quad` return path | concave-quad geometry path used by AMR/hydro children info |
| `idCPara`/`idEPara`/`idCNPara` uninitialized | `quadrant_cell_minmod_estimate_callback`, `quadrant_edge_minmod_estimate_callback`, `quadrant_corner_minmod_estimate_callback`, `quadrant_set_gradient_zero_estimate_callback` |
| `distance` uninitialized | `IOCallbacks::quadrant_write_distance_profiles_callback` |
| format extra args | half-time variable illegal-state messages |
| inline variables | `core/trace.h`, `nodal/face_geometry_mirror.h`, `nodal/matrix_accessor.h` |

## Gate Closure

```text
package: M11.0
base commit: 9e5f55e
focused verification: warning inventory captured and classified
G0: reused from M10L.10 clean build PASS
G1: reused from M10L.10 serial golden PASS
G3: reused from M10L.10 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
