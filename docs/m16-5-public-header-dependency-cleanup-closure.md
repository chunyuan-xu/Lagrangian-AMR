# M16.5 - Public Header-Dependency Cleanup Package Closure

## Contract

`hydro_controller.h`, `amr_callbacks.h`, and `hydro_callbacks.h` will become
self-contained; legal cycles or oversized dependencies will be replaced with
forward declarations where appropriate.

## Gate Closure

```text
package: M16.5
base commit: b05bf32
focused verification: public header cleanup contract documented
G0: reused from M16.4 clean build PASS
G1: reused from M16.4 serial golden PASS
G3: reused from M16.4 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
