# M8.2.2 门禁记录：通量/状态更新回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.2.2 基线：`84d7f44`（M8.2.1）
- 生产改动：`src/hydro/hydro_callbacks.h` 追加 `HydroCallbacks::quadrant_compute_halftime_variable_callback`、`quadrant_parent_edge_matrix_callback`、`quadrant_accept_center_solution_callback`、`quadrant_compute_corner_force_callback`、`quadrant_flux_relaxed_reset_callback`、`generate_children_info_from_parent`（自 main.cpp 逐字迁入）；`src/main.cpp` 移除 6 个本地函数（369 行），注册与 `generate_children_info_from_parent` 调用点路由到 `HydroCallbacks::`。
- 未修改：半步积分、EOS、受力组装、中心接受、parent-edge 公式、ghost 边界层。
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
| Noh Uniform | 0 | 0 | PASS | 13.1 s |
| Sod AMR | 0 | 0 | PASS | 49.2 s |
| Sedov AMR | 0 | 0 | PASS | 36.4 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 25.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.6 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.2.2 的 G0、G1、G3 全部通过。半步积分/状态接受/受力组装等回调已剥离到 `HydroCallbacks`，ghost 边界层受力组装与跨节点积分回退通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
