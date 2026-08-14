"""Shared constants for the golden-gate runners and VTU/PVTU comparator.

Single source of truth for the numerical comparison tolerance used by
python/compare_vtu.py, python/run_tests.py (G1) and python/run_mpi_gates.py
(G3). The value was relaxed from 1e-12 to 1e-6 on 2026-08-14: different
machines' UCRT math libraries (pow/sin/cos) introduce ~1 float32 ULP
(~4.8e-7) of last-bit rounding, so 1e-12 (bit-exactness) cannot be met
across machines. 1e-6 absorbs that noise while still catching real
regressions (a genuine bug changes results by far more than 1e-6).
"""

GATE_TOLERANCE = 1e-6
