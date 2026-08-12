# M9.3.3 门禁记录：IO 统计诊断壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.3.3 基线：`a25086e`（M9.3.2）
- 生产改动：
  - `src/io/io_callbacks.h` 追加 `IOCallbacks::write_distance_profiles`（23 行，距离剖面输出）、`IOCallbacks::StatTotalEnergyError`（55 行，全局总能量守恒检查，MPI Allreduce + abort 门禁）；
  - `src/main.cpp` 移除 2 个函数（约 63 行），3 处调用点路由到 `IOCallbacks::`（`execute_amr` 的 `energy_fn` 参数、`advance_time_step` 内 1 处 `StatTotalEnergyError`、1 处 `write_distance_profiles`）。
- 未修改：能量守恒阈值、MPI 归约、距离剖面输出逻辑。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 60.0 s |
| Sedov AMR | 0 | 0 | PASS | 47.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.3.3 的 G0、G1、G3 全部通过。IO 统计诊断壳（距离剖面、总能量守恒检查）已剥离到 `IOCallbacks`，能量守恒阈值与 MPI 归约行为不变，参数恢复，reference 未变化。main.cpp 从 535 减至 472 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
