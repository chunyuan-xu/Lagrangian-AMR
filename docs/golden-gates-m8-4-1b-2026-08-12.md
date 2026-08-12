# M8.4.1b 门禁记录：main.cpp 最终闭环审计（2026-08-12）

## 基线与范围

- 分支：main
- M8.4.1b 基线：`b1863bb`（M8.4.1a）
- main.cpp 从约 4700 行瘦身至 2455 行。

## 清理审计

- **零引用 static**：`zero-use: NONE`——剥离后无遗留死代码；
- **冗余 include 审计**：
  - `p4est_iterate`（36 处直接调用）、`p4est_vtk`（13 处直接调用）为 main.cpp 必需；
  - `p4est_bits/extended/io/communication` 与 `p8est_*` 均为 `#ifndef/#ifdef P4_TO_P8` 条件编译的 p4est API 层（`p4est_iterate`/`p4est_vtk_*` 依赖），移除会导致未声明错误；
  - 无冗余 include 可安全删除。
- **编排梳理**：M7.4 已建立 `Simulation::run` 高层编排；main.cpp 主循环清晰调用 `AMRController`/`RiemannPhases`/`HydroPhases`/`AMRCallbacks`/`HydroCallbacks`/`IOCallbacks`/`Diagnostics` 模块，CFD 流程全景可见。

## 结论

M8.4.1 最终闭环审计通过：main.cpp 已剥离全部 22 个 `quadrant_` 回调到 AMR/Hydro/IO 模块，无死代码，无冗余 include。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 |
|---|---:|---:|---:|
| Noh Uniform | 0 | 0 | PASS |
| Sod AMR | 0 | 0 | PASS |
| Sedov AMR | 0 | 0 | PASS |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 |
|---|---:|---:|---:|---:|
| Sod AMR | 4 | 0 | 0 | PASS |
| Sedov AMR | 4 | 0 | 0 | PASS |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M8 结论

M8 全部阶段（8.1 AMR / 8.2 Hydro / 8.3 IO-Diagnostics / 8.4 最终闭环）完成。main.cpp 不再承载任何 `quadrant_` 回调，全部剥离到 `AMRCallbacks`/`HydroCallbacks`/`IOCallbacks` 模块。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
