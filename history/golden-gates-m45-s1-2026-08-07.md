# M4.5 S1 门禁记录：零引用 AMR 旧代码删除（2026-08-07）

## 基线与范围

- 分支：main
- S1 基线：`ab26d9a`（M4.5 计划）
- 生产改动：仅 `src/main.cpp`，删除 8 个零引用死函数（共 272 行）：
  - `GetRefineCornerCoords`、`GetRefineCornerVelos`
  - `Lagrangian_coarsen_init_condition`
  - `get_quadrant_boundary_from_p4est`
  - `quadrant_copy_cell_variable_to_array_callback`
  - `write_balance_solution`、`write_coarsen_solution`、`write_refine_solution`
- 删除后零残留（`grep` 计数 0），对应 unused-function 警告消除。
- 未修改：活动回调、AMRController/Policy/Transfer、主循环编排、守恒量。
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
| Noh Uniform | 0 | 0 | PASS | 19.0 s |
| Sod AMR | 0 | 0 | PASS | 58.7 s |
| Sedov AMR | 0 | 0 | PASS | 47.2 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## S1 结论

S1 的 G0、G1、G3 全部通过。8 个零引用旧函数已删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
