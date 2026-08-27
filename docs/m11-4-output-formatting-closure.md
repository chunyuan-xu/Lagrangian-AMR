# M11.4 - Output Formatting and Initialization Package Closure

## Change

- Removed the extra `P4EST_DIM` arguments from the half-time illegal-state
  messages in `src/hydro/hydro_callbacks.h`.
- Initialized `distance = 0.` in
  `quadrant_write_distance_profiles_callback` and added a fail-closed abort for
  unsupported distance profile types before any file write.

## Focused Verification

```text
C:/msys64/ucrt64/bin/python.exe python/test_m11_4_output_formatting.py --summary .tmp/mg-m11-4-output-formatting.json
MG-M11-4 PASS
```

## Gate Closure

```text
package: M11.4
base commit: a1b4736
focused verification: MG-M11-4 PASS
G0: make clean && make -j8 PASS; bin/AMR_Solver.exe present
G1: serial_golden_summary.json status PASS (Noh, Sod, Sedov)
G3: mpi_gate_summary.json status PASS after one known flaky Sedov rerun
param_restored: true
reference hash: unchanged
closure commit: this package's single HEAD commit
```

## Known Flake

The first G3 run failed on Sedov 4-rank with the historical
`Time step is too small` / `0xc0000409` pattern. A rerun passed both Sod and
Sedov. No reference or tolerance was changed.
