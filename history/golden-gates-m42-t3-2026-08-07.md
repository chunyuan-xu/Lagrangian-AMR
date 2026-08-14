# M4.2 T3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- T3 基线：`7b0dab6`（T1b）
- M4.2 代码范围：T1a（coarsen 接口抽取）、T1b（refine 接口抽取）；T2/T3 为静态审计与收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## T2：守恒与语义核对（静态审计）

- coarsen 接口与旧实现字段完全一致（`cell`/`corner_vector`/`ChildrenCnGeomVara`/`ChildrenPhysicalVara` 共 14 项交集，无遗漏无新增）；
- refine 接口与旧分发块字段一致，无遗漏无新增；
- 质量/总能量守恒校验逻辑保留在 adapter 内，未改动。

## T3：adapter 收口审计

- `Lagrangian_replace_quads` 现为纯 adapter：coarsen 分支调 `AMRTransfer::coarsen_children_to_parent`，refine 分支保留 trace 诊断并调 `AMRTransfer::refine_distribute_buffers`；
- 无内联转移数值残留；`p4est_refine/coarsen/balance_ext` 注册不变；
- 辅助 `generate_children_info_from_parent` 仍被 refine 分支使用，保留。

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
| Noh Uniform | 0 | 0 | PASS | 19.7 s |
| Sod AMR | 0 | 0 | PASS | 61.0 s |
| Sedov AMR | 0 | 0 | PASS | 48.3 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.2 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.2 结论

M4.2 AMRTransfer 抽取完成：coarsen 聚合与 refine 分发已抽取为 `AMRTransfer` 纯接口，p4est 回调保留为 adapter，守恒语义不变。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
