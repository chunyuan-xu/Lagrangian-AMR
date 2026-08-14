# M9.1.4 门禁记录：AMR 误差估计器剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.1.4 基线：`3eae572`（M9.1.3）
- 生产改动：
  - `src/amr/amr_callbacks.h` 追加 `AMRCallbacks::Lagrangian_refine_err_estimate`、`Lagrangian_coarsen_err_estimate`（p4est refine/coarsen 误差估计回调，薄包装 `AMRAgorithm::RefineErrorEstimate`/`CoarsenErrorEstimate`）；
  - `src/main.cpp` 移除 2 个本地函数（约 11 行），调用点路由到 `AMRCallbacks::`；
  - `amr_callbacks.h` 补 `amr/amr_criteria.h` include（`AMRAgorithm` 定义处）。
- 未修改：refine/coarsen 判据逻辑。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 61.2 s |
| Sedov AMR | 0 | 0 | PASS | 47.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.9 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.1.4 的 G0、G1、G3 全部通过。AMR 误差估计器已剥离到 `AMRCallbacks`，refine/coarsen 判据行为不变，参数恢复，reference 未变化。main.cpp 从 1186 减至 1178 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
