# M4.0 B5 门禁记录：edge minmod 指针迁移（2026-08-06）

## 基线与范围

- 分支：main
- B5 基线：`8c0832e`（B4）
- 生产改动：仅 `src/main.cpp`
- 迁移回调：`quadrant_edge_minmod_estimate_callback`
- 修改内容：
  - 回调体悬挂分支中剩余 6 处 `ghost_data[...]` 解引（child1/child2/parent 的 data 与 cndata）统一改为 `context->session->remote()`；
  - 删除已无用的 `ghost_data = context->session->data();` 声明。
- 未修改：minmod 梯度公式、owner 写入门禁（write_vara 仅在非 ghost 赋值）、遍历顺序、ghost 生命周期。
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
| Noh Uniform | 0 | 0 | PASS | 18.8 s |
| Sod AMR | 0 | 0 | PASS | 58.5 s |
| Sedov AMR | 0 | 0 | PASS | 46.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## B5 结论

B5 的 G0、G1、G3 全部通过。edge minmod 回调的幽灵层读取已统一迁移为 `remote()` 只读访问，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
