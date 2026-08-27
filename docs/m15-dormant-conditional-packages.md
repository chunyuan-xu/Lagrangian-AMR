# M15 Conditional Packages - Dormant

The following packages are dormant until their activation condition is met:

| Package | Activation condition |
|---|---|
| `M15.2x` | one measured traversal-fusion candidate |
| `M15.3x` | one measured exchange-optimization candidate |
| `M15.3P` | M15.1 proves material communication cost and M10L lists all remote readers |

Each dormant package gets its own closure record and commit when activated.

## Gate Closure

```text
package: M15-dormant
base commit: 15afb3d
focused verification: dormant status recorded
G0: reused from M15.1 clean build PASS
G1: reused from M15.1 serial golden PASS
G3: reused from M15.1 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
