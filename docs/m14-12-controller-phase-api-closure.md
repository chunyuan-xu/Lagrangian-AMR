# M14.12 - Controller Phase API Package Closure

## Contract

`HydroController::advance_single_stage` names the legacy phase sequence:

```text
boundary -> half-time -> corner geometry -> nodal mirror/local master
  -> exchange -> AMR hanging/parent-edge -> exchange -> Riemann
  -> divergence -> coordinate -> density -> momentum -> work -> energy -> EOS -> sound-speed
```

The next step is to route `advance_single_stage` through named phase functions
without changing phase order.

## Gate Closure

```text
package: M14.12
base commit: 41ecc52
focused verification: controller phase API contract documented
G0: reused from M14.11 clean build PASS
G1: reused from M14.11 serial golden PASS
G3: reused from M14.11 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
