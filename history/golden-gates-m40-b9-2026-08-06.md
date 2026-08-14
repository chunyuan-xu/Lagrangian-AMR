# M4.0 B9 门禁记录：hanging matrix 指针迁移（2026-08-06）

## 基线与范围

- 分支：main
- B9 基线：`21ebe76`（B8）
- 生产改动：仅 `src/main.cpp`
- 迁移回调：`quadrant_hanging_point_matrix_assemble_callback`
- 修改内容：
  - 回调体三处 `ghost_data[...]` 解引（quad / quad_aside / quad_full）→ `context->session->remote()`；
  - 删除 `ghost_data` 声明；
  - 注册：`ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes` 中该回调的 user_data 由 `(void*)session.data()` 改为复用函数顶部 `callback_context`。
- 修正：首次因重复/前置声明 `callback_context` 引入编译问题，调整为在函数顶部声明一次、两个悬挂回调注册共用后通过。
- 未修改：MatrixP/RHS 组装、owner 写入门禁、trace、exchange 时序、ghost 生命周期。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次尝试因 context 声明顺序问题失败，不作为有效门禁。
- 修正后有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 58.4 s |
| Sedov AMR | 0 | 0 | PASS | 46.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## B9 结论

B9 的有效 G0、G1、G3 全部通过。hanging matrix 回调的幽灵层读取已迁移为 `remote()` 只读访问，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
