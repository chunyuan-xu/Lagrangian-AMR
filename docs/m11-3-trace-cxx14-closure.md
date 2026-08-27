# M11.3 - C++14 Trace-State Package Closure

## Change

Replaced the C++17 `inline int g_trace_riemann_iter` with a C++14 accessor:

```cpp
inline int &trace_riemann_iter();
```

Migrated all readers/writers in `hydro_controller.h` and `hydro_callbacks.h`
to `trace_riemann_iter()`.

Removed `g_trace_snapshot_stage` entirely. `trace_target_snapshot` now passes
the stage string as explicit `p4est_iterate` callback context, and
`trace_target_snapshot_callback` reads it from `user_data`.

## Multi-TU Linkage

Added `python/test_m11_3_trace_cxx14.py`, which compiles two translation units
that both include `core/trace.h`, calls the accessor from one TU, and reads it
from the other. This proves the accessor approach has no C++17 inline-variable
ODR/linkage problem under `-std=c++14`.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m11_3_trace_cxx14.py --summary .tmp/mg-m11-3-trace-cxx14.json
MG-M11-3 PASS
```

## Gate Closure

```text
package: M11.3
base commit: 7f95176
focused verification: MG-M11-3 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
