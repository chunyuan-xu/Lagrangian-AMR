# M14.1 - Local Corner-Matrix Package Closure

## Kernel Contract

`HydroCallbacks::quadrant_corner_matrix_assemble_callback` computes, per corner
`k`:

- reconstruct density/pressure/velocity from cell fields;
- `DeltaU[k]` and impedance fields on `m_cndata`;
- `MarCnData[idcnMcp][k]` from `Zcp/Rcp/Lcp/Ncp` (with Rotated-solver
  diagonalization);
- `idcnRHS[k] = LcpNcpPc + idcnMcpUc`.

## Ownership

The kernel is owner-local: it writes only the current quadrant's `CVariable`
and `m_cndata`. No ghost write occurs.

## Migration Boundary

The next step is to extract this per-corner algebra into a pure
`build_corner_matrix_rhs(...)` helper. This package records the input/output
contract so the extraction can preserve exact formulas and no exchange
changes.

## Gate Closure

```text
package: M14.1
base commit: e101f59
focused verification: local corner-matrix kernel contract documented
G0: reused from M14.0 clean build PASS
G1: reused from M14.0 serial golden PASS
G3: reused from M14.0 MPI golden PASS
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
