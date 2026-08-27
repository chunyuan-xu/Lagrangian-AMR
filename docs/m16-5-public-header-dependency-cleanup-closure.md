# M16.5 - Public Header-Dependency Cleanup Package Closure

## Contract

`hydro_controller.h`, `amr_callbacks.h`, and `hydro_callbacks.h` are now
self-contained (each compiles alone with `-std=c++14`). Legal cycles are
handled with forward declarations where appropriate.

Verified by standalone compile checks for all three headers.

Changed:

- `src/hydro/hydro_callbacks.h` — forward-declared
  `AMRCallbacks::get_hanging_edge_info_from_logical_position` to keep the
  header self-contained without introducing the `amr_callbacks.h` include
  cycle.

## Gate Closure

```text
package: M16.5
base commit: 101c52f
focused verification: public headers self-contained and forward-declared
G0: clean build PASS
G1: serial golden PASS
G3: MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
