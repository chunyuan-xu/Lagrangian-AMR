# M4.1 A1 门禁记录：死 AMR 判据移除（2026-08-07）

## 基线与范围

- 分支：main
- A1 基线：`313f4b1`（M4.1 计划）
- 生产改动：仅 `src/main.cpp`
- 删除内容（四个未注册、零调用的死判据）：
  - `Lagrangian_init_refine_err_estimate`
  - `Lagrangian_refine_fixed_estimate`
  - `Lagrangian_coarsen_fixed_estimate`
  - `Lagrangian_init_coarsen_err_estimate`
- 保留：已委托 `AMRAgorithm` 的 `Lagrangian_refine_err_estimate` / `Lagrangian_coarsen_err_estimate`（主循环实际注册）。
- 未修改：AMR 判据语义、`p4est_refine_ext`/`p4est_coarsen_ext` 注册、ghost 生命周期、owner/remote 语义。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS
- 删除后消除了对应的 unused-function 警告；仅剩既有 warning。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 20.4 s |
| Sod AMR | 0 | 0 | PASS | 66.7 s |
| Sedov AMR | 0 | 0 | PASS | 51.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## A1 结论

A1 的 G0、G1、G3 全部通过。四个未注册死判据已删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
