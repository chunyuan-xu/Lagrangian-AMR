# M9.1.2 门禁记录：AMR 拓扑编排壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.1.2 基线：`b78f319`（M9.1.1）
- 生产改动：`src/amr/amr_callbacks.h` 追加 `AMRCallbacks::Get_AMR_BDY_info`、`append_refresh_snapshot`、`refresh_after_balance` 及 3 个补充回调（`quadrant_reset_parent_edge_callback`、`quadrant_get_children_hanging_info_callback`、`quadrant_reset_hanging_info_callback`）与前置声明；`src/main.cpp` 移除 6 个本地函数，调用点路由到 `AMRCallbacks::`。
- 修正：G0 首次因 `Get_AMR_BDY_info` 引用晚定义的 3 个回调失败，添加前置声明后通过。
- 未修改：balance 后刷新、幂等检查、AMR BDY 信息交换时序。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因前置声明缺失失败，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 62.7 s |
| Sedov AMR | 0 | 0 | PASS | 48.3 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.1.2 的 G0、G1、G3 全部通过。AMR 拓扑编排壳（refresh/AMR BDY/幂等快照）已剥离到 `AMRCallbacks`，并行分裂/合并与悬点修复计算边界不变，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
