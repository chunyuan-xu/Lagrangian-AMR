# M4.0 B7 门禁记录：set-init-parent-edge 指针迁移（2026-08-06）

## 基线与范围

- 分支：main
- B7 基线：`f446c34`（B6）
- 生产改动：仅 `src/main.cpp`
- 迁移回调：`quadrant_set_init_parent_edge_callback`
- 修改内容：
  - 回调体 7 处 `ghost_data[...]` 解引迁移：
    - `m_child1_data` / `m_child2_data` / `m_parent_data` 改用 `context->session->data() + id`（保留 Ncp 预减写路径）；
    - `m_child*_vara` / `m_child*_cndata` 改用 `context->session->remote(id)` 只读访问；
  - 删除未使用的 `p4est_data` 与 `ghost_data` 声明；
  - 注册：`Get_AMR_BDY_info` 中该回调的 user_data 由 `(void*)session.data()` 改为 `&GhostCallbackContext{&session}`。
- 未修改：Ncp 预减、Lcp 计算、owner 写入门禁、遍历顺序、ghost 生命周期。
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
| Noh Uniform | 0 | 0 | PASS | 19.1 s |
| Sod AMR | 0 | 0 | PASS | 58.7 s |
| Sedov AMR | 0 | 0 | PASS | 46.4 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.4 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## B7 结论

B7 的 G0、G1、G3 全部通过。set-init-parent-edge 回调的幽灵层读取已迁移为 `remote()` 只读访问，写路径经 `data()` 保留，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
