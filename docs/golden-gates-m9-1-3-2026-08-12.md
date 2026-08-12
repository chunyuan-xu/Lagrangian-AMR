# M9.1.3 门禁记录：AMR replace 回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.1.3 基线：`135679a`（M9 计划修订）
- 生产改动：
  - `src/amr/amr_callbacks.h` 追加 `AMRCallbacks::Lagrangian_replace_quads`（216 行，p4est refine/coarsen replace 回调，父子网格数据插值）；
  - `src/main.cpp` 移除该函数（约 192 行），调用点路由到 `AMRCallbacks::Lagrangian_replace_quads`；
  - `amr_callbacks.h` 补 `amr/amr_transfer.h`、`core/trace.h` include；因与 `hydro_callbacks.h` 构成 include 循环（hydro 引用 `AMRCallbacks::`、amr 引用 `HydroCallbacks::generate_children_info_from_parent`），改为对 `HydroCallbacks::generate_children_info_from_parent` 前置声明，不 include 整个 `hydro_callbacks.h`。
- 修正：G0 首次因删除函数体时残留孤立 `static void` 报错，删除后通过；循环 include 修正后通过。
- 未修改：refine/coarsen 数据插值逻辑、能量/质量守恒检查、trace 诊断分支。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因残留孤立 `static void` 失败，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 60.6 s |
| Sedov AMR | 0 | 0 | PASS | 47.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.1.3 的 G0、G1、G3 全部通过。AMR replace 回调（父子网格数据插值）已剥离到 `AMRCallbacks`，refine/coarsen 与 balance 的数据传递行为不变，参数恢复，reference 未变化。main.cpp 从 1378 减至 1186 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
