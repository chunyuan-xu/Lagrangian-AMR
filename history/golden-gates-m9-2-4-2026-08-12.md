# M9.2.4 门禁记录：MUSCL 角梯度回调与 Hydro 壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.2.4 基线：`3e0f659`（M9.2.3）
- 生产改动：
  - `src/hydro/hydro_callbacks.h` 追加 `HydroCallbacks::quadrant_set_gradient_zero_estimate_callback`、`quadrant_corner_minmod_estimate_callback`（MUSCL 梯度限制器底层回调）；
  - `src/hydro/hydro_controller.h` 追加 `HydroController::Gradient_estimate`（MUSCL 梯度壳）、`PreProcess`（默认标记）；
  - `src/main.cpp` 移除 4 个函数（约 194 行），调用点路由到 `HydroController::PreProcess`；`Gradient_estimate` 内部回调引用改为 `HydroCallbacks::`。
- 未修改：梯度限制器公式、refine_coarsen_enum 分支、ghost 读写时序、PreProcess 默认标记逻辑。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.3 s |
| Sod AMR | 0 | 0 | PASS | 60.7 s |
| Sedov AMR | 0 | 0 | PASS | 47.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.2.4 的 G0、G1、G3 全部通过。MUSCL 角梯度回调与 `Gradient_estimate`/`PreProcess` 壳已剥离到 `HydroCallbacks`/`HydroController`，梯度限制器公式与 ghost 读写时序不变，参数恢复，reference 未变化。main.cpp 从 992 减至 798 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
