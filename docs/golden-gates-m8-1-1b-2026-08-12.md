# M8.1.1b 门禁记录：minmod 回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.1.1b 基线：`a35ceb0`（M8.1.1a）
- 生产改动：
  - `src/amr/amr_callbacks.h`：追加 `AMRCallbacks::get_hanging_edge_info_from_logical_position`、`quadrant_edge_minmod_estimate_callback`、`quadrant_cell_minmod_estimate_callback`（自 main.cpp 逐字迁入）；
  - 新建 `src/mesh/ghost_context.h`：`GhostCallbackContext` 共享结构（main.cpp 原定义迁出）；
  - `src/main.cpp`：移除 3 个本地函数（409 行）与 `GhostCallbackContext` 定义；5 处 `get_hanging_edge_info` 调用点与 2 处 minmod 注册路由到 `AMRCallbacks::`。
- 未修改：minmod 梯度公式、AMR 判据、回调顺序。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因 `GhostCallbackContext` 未在 header 可见失败，迁移到 `ghost_context.h` 后通过。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 12.1 s |
| Sod AMR | 0 | 0 | PASS | 49.3 s |
| Sedov AMR | 0 | 0 | PASS | 36.1 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.9 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.1.1b 的 G0、G1、G3 全部通过。minmod 回调与 hanging 几何辅助已剥离到 `AMRCallbacks`，`GhostCallbackContext` 迁入共享 header，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
