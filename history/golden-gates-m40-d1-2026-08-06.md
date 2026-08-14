# M4.0 D1 门禁记录：ghost_data 声明清零（2026-08-06）

## 基线与范围

- 分支：main
- D1 基线：`a772570`（B10）
- 生产改动：仅 `src/main.cpp`
- 修改内容：
  - 删除 `quadrant_whether_allowing_coarsening_from_edge_callback` 中从未解引的 `ghost_data = (quad_data_t *)user_data;` 声明；
  - 注册：`set_allowing_coarsening_tag` 中该回调的 user_data 由 `session.data()` 改为复用 `&callback_context`（与 corner 回调共用）。
- 结果：全仓 `ghost_data` 零残留（`grep -c ghost_data src/main.cpp` = 0）。
- 未修改：coarsening 判据、owner 写入门禁、遍历顺序、ghost 生命周期。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS
- `ghost_data` 残留计数：0

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.5 s |
| Sod AMR | 0 | 0 | PASS | 58.0 s |
| Sedov AMR | 0 | 0 | PASS | 47.1 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.2 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## D1 结论

D1 的 G0、G1、G3 全部通过。全仓 `ghost_data` 裸占位符声明已清零，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
