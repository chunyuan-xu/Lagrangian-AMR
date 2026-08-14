# M9.1.1 门禁记录：AMR 标记壳函数剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.1.1 基线：`17d5d06`（M9 计划）
- 生产改动：
  - `src/amr/amr_callbacks.h` 追加 `AMRCallbacks::set_default_refining_tag`、`set_default_coarsening_tag`、`set_allowing_coarsening_tag`（自 main.cpp 迁入）；
  - 补充剥离 3 个 M8 遗漏的 tag 回调（`quadrant_set_default_refining_tag_callback`、`quadrant_set_default_coarsening_tag_callback`、`quadrant_whether_allowing_coarsening_from_corner_callback`）；
  - `src/main.cpp` 移除 6 个函数（140 行），调用点与函数指针路由到 `AMRCallbacks::`。
- 未修改：refine/coarsen 标记逻辑、AMR 编排、owner 门禁。
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
| Noh Uniform | 0 | 0 | PASS | 15.0 s |
| Sod AMR | 0 | 0 | PASS | 49.5 s |
| Sedov AMR | 0 | 0 | PASS | 37.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 27.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 20.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.1.1 的 G0、G1、G3 全部通过。AMR 标记壳函数已剥离到 `AMRCallbacks`，并行分裂/合并标记行为不变，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
