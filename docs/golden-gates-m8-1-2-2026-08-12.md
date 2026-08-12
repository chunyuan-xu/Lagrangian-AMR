# M8.1.2 门禁记录：拓扑/悬点回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.1.2 基线：`f4dd252`（M8.1.1b）
- 生产改动：`src/amr/amr_callbacks.h` 追加 `AMRCallbacks::quadrant_whether_allowing_coarsening_from_edge_callback`、`quadrant_update_after_balance_callback`、`quadrant_set_init_parent_edge_callback`（自 main.cpp 逐字迁入）；`src/main.cpp` 移除 3 个本地回调（379 行），注册路由到 `AMRCallbacks::`。
- 未修改：coarsening 判据、balance 后悬点修复、parent-edge 组装公式、exchange 时序。
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
| Noh Uniform | 0 | 0 | PASS | 17.7 s |
| Sod AMR | 0 | 0 | PASS | 60.1 s |
| Sedov AMR | 0 | 0 | PASS | 41.5 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 22.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.1.2 的 G0、G1、G3 全部通过。AMR 拓扑/悬点回调已剥离到 `AMRCallbacks`，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
