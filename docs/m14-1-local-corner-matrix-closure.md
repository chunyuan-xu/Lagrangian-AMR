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

## Implementation

Added `src/hydro/corner_matrix_kernel.h` with
`HydroCallbacks::build_corner_matrix_rhs(...)` and migrated
`quadrant_corner_matrix_assemble_callback` to call it per corner. The
callback keeps only the trace output after the kernel call.

Added `python/test_m14_1_corner_matrix_kernel.py` to fixture the kernel.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m14_1_corner_matrix_kernel.py --summary .tmp/mg-m14-1-corner-matrix-kernel.json
MG-M14-1 PASS
```

## Gate Closure

```text
package: M14.1
base commit: e101f59
focused verification: MG-M14-1 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank)
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```
