# M4.2 T1a 门禁记录：coarsen transfer 接口抽取（2026-08-07）

## 基线与范围

- 分支：main
- T1a 基线：`823575e`（M4.2 计划）
- 生产改动：
  - 新建 `src/amr/amr_transfer.h`：`AMRTransfer::coarsen_children_to_parent`，将 `Lagrangian_replace_quads` coarsen 分支逐字搬入；
  - `src/main.cpp`：coarsen 分支体（约 155 行）替换为对该接口的调用；添加 include。
- 未修改：所有聚合数值公式与字段顺序、守恒量、`p4est_refine/coarsen/balance_ext` 注册、ghost 生命周期。
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
| Noh Uniform | 0 | 0 | PASS | 19.5 s |
| Sod AMR | 0 | 0 | PASS | 61.1 s |
| Sedov AMR | 0 | 0 | PASS | 48.5 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.6 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## T1a 结论

T1a 的 G0、G1、G3 全部通过。coarsen children→parent 聚合已抽取为 `AMRTransfer` 纯接口，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
