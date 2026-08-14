# M8.4.1a 门禁记录：剩余 hydro 回调清零（2026-08-12）

## 基线与范围

- 分支：main
- M8.4.1a 基线：`182cc06`（M8.3.2）
- 生产改动：`src/hydro/hydro_callbacks.h` 追加 5 个回调（`quadrant_compute_RcpLcpNcp_callback`、`quadrant_compute_relaxed_info_callback`、`quadrant_relaxed_hanging_solver_callback`、`quadrant_get_BYD_callback`、`quadrant_update_corner_coordinate_callback`）；`src/main.cpp` 移除本地定义（410 行），注册路由到 `HydroCallbacks::`。
- 结果：main.cpp 中 `static void quadrant_` 回调清零（0 个）。
- 未修改：Rcp/Lcp/Ncp 计算、relaxed 信息、悬点求解、BYD、corner 坐标更新公式。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 13.2 s |
| Sod AMR | 0 | 0 | PASS | 51.3 s |
| Sedov AMR | 0 | 0 | PASS | 38.8 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.4.1a 的 G0、G1、G3 全部通过。main.cpp 中 `quadrant_` 回调全部清零（22 个原回调全部剥离到 AMRCallbacks/HydroCallbacks/IOCallbacks），参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
