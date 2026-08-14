# M9.2.3 门禁记录：Hydro 单阶段推进壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.2.3 基线：`854d066`（M9.1.4）
- 生产改动：
  - `src/hydro/hydro_controller.h` 追加 `HydroController::advance_single_stage`（M9.2.2 跳过项，Hydro 大流水线：边界 → 半步 → corner 矩阵/速度 → 散度 → 坐标 → 守恒更新）；
  - `src/main.cpp` 移除 `advance_single_stage`（约 107 行），调用点路由到 `HydroController::`；
  - 依赖一并迁出：`StatGlobalFieldChecksum` → `src/io/io_callbacks.h` `IOCallbacks`（M9.3.3 提前完成，advance_single_stage 内部 9 处调用改为 `IOCallbacks::`）；`trace_target_snapshot` 三件套 → `src/core/trace.h`（诊断归属）；
  - `hydro_controller.h` 补 `physics/stage_policy.h`、`solver/solver_gate.h`、`init/initializer.h` include；`io_callbacks.h` 补 `core/trace.h`；`trace.h` 补 `variable.h`。
- 范围边界：`Gradient_estimate`/`PreProcess` 依赖 M9.2.4 的 `quadrant_corner_minmod_estimate_callback` 等回调，留待 M9.2.4 一并迁移。
- 修正：G0 首次因残留孤立 `static void` 与重复声明失败，清理后通过。
- 未修改：单阶段推进阶段顺序、Riemann 循环、MPI 通信时序、诊断开关逻辑。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因残留孤立 `static void` 与 `quadrant_corner_minmod_estimate_callback` 重复声明失败，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 60.1 s |
| Sedov AMR | 0 | 0 | PASS | 47.8 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.2.3 的 G0、G1、G3 全部通过。单阶段 Hydro 推进壳已剥离到 `HydroController`，其依赖 `StatGlobalFieldChecksum` 归 `IOCallbacks`、`trace_target_snapshot` 归 `core/trace.h`。Riemann 循环与 MPI 通信/分发流程未中断，参数恢复，reference 未变化。main.cpp 从 1178 减至 992 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
