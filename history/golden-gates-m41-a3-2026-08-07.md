# M4.1 A3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- A3 基线：`35693fa`（A1）
- M4.1 代码范围：A1 死判据移除；A2/A3 为静态审计与收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## A2：委托核对（静态审计）

- `main.cpp` 中 `Lagrangian_refine_err_estimate`（816）与 `Lagrangian_coarsen_err_estimate`（822）仅做委托，无内联判据逻辑；
- `AMRAgorithm::RefineErrorEstimate` / `CoarsenErrorEstimate` 覆盖 `PressureGradient`/`DensityGradient`/`Distance` 全分支，以及 `minus_level`/`max_level` 边界与 `refine_err`/`coarsen_error` 阈值；
- `main.cpp` 中不再有其它 `static int Lagrangian*` 内联判据（仅剩 init 条件 `Lagrangian_coarsen_init_condition`）。

## A3：判据覆盖审计

- 主循环 `p4est_refine_ext`（5213）注册 `Lagrangian_refine_err_estimate`；
- 主循环 `p4est_coarsen_ext`（5228）注册 `Lagrangian_coarsen_err_estimate`；
- 配合 `set_default_*_tag` 与 `set_allowing_coarsening_tag` 门禁，无遗漏的内联判据残留。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 20.1 s |
| Sod AMR | 0 | 0 | PASS | 62.4 s |
| Sedov AMR | 0 | 0 | PASS | 48.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 28.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 28.0 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.1 结论

M4.1 AMR 判据模块化完成：死判据移除、委托核对、覆盖审计全部通过。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
