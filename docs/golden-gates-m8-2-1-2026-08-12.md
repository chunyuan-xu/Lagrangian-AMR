# M8.2.1 门禁记录：角点矩阵/速度回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.2.1 基线：`6b5b19c`（M8.1.2）
- 生产改动：
  - 新建 `src/hydro/hydro_callbacks.h`：`HydroCallbacks::quadrant_corner_matrix_assemble_callback`、`quadrant_corner_velocity_callback`、`quadrant_copy_velocity_from_lag_to_relax_callback`、`convert_which_corner_to_user_define_index`（自 main.cpp 逐字迁入）；
  - 新建 `src/core/trace.h`：trace 工具（`target_trace_enabled`/`is_trace_*`/`open_corner2_trace`/`trace_matrix`/`trace_vector`/`g_trace_riemann_iter` 等）共享化；
  - `src/main.cpp`：移除 4 个函数与 trace 工具（共 535 行），注册/调用点路由到 `HydroCallbacks::`。
- 修正：G0 三次失败（`corner_solve.h` 路径、trace 工具未共享、`corner_velocity` 末尾参数注册未路由），均修复后通过。
- 未修改：矩阵装配、owner 求解、边界检测、copy 逻辑、浮点次序。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 三次尝试因 include 路径与未路由调用点失败，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 17.6 s |
| Sod AMR | 0 | 0 | PASS | 62.9 s |
| Sedov AMR | 0 | 0 | PASS | 50.8 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.2.1 的 G0、G1、G3 全部通过。角点矩阵/速度回调已剥离到 `HydroCallbacks`，trace 工具共享化，owner-compute 策略在四进程下未受影响，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
