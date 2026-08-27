# M10L.1 - ABI and Definedness Fixture Package Closure

## Hypothesis

The current `quad_data_t` ABI is the authoritative fixed-size leaf record, and
at least one required production writer can be protected by a focused
poison/parity fixture so that removing that writer is caught by the focused
micro-gate before broader regression.

## Focused Change

Added `python/test_m10l1_abi_definedness.py`, which:

- reasserts the current `sizeof`, `alignof`, and member offsets for
  `quad_data_t` and nested scalar types;
- poisons every slot of `quad_data_t::nodal.master`;
- runs the required production writer `Nodal::write_cell_local_master`;
- asserts every poisoned slot is overwritten and that the unit-square fixture
  matches the known legacy result.

No production source, reference asset, tolerance, or golden field selection was
changed.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m10l1_abi_definedness.py --summary .tmp/mg-m10l1-abi-definedness.json
MG-M10L1 PASS
```

## Gate Closure

```text
package: M10L.1
base commit: 697d7bf
focused verification: MG-M10L1 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS (Sod 4-rank, Sedov 4-rank after one flaky rerun)
param_restored: true
reference hash: unchanged (17 reference files, no diff)
closure commit: this package's single HEAD commit
```

## Known Flake

The first G3 run for this package failed on Sedov 4-rank with the historical
`Time step is too small in quad 0` / `0xc0000409` pattern. No production source
changed in this package. A rerun passed both Sod and Sedov, matching the
historical "consecutive full G3 reproduction" closure practice recorded in
`history/golden-gates-dod-sedov-2026-08-12.md`.
