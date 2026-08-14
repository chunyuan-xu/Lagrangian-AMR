# M4.4 C3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- C3 基线：`1bbcaa0`（C2）
- M4.4 代码范围：C1（AMR 编排抽取）、C2（partition 编排抽取）；C3 审计与收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## C3：controller 收口审计

- 主循环 AMR 编排已全部委托 `AMRController::execute_amr`（5010）与 `AMRController::execute_partition`（5023）；
- 无 `p4est_refine/coarsen/balance_ext` 内联残留；
- `main.cpp:5264` 的 `p4est_partition` 为初始化初始 partition（`partforcoarsen`），非周期 partition，按计划保留；
- 阶段顺序（refine→rebuild→coarsen tag→coarsen→balance→destroy；partition→destroy）与 p4est 参数不变。

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
| Sod AMR | 0 | 0 | PASS | 60.2 s |
| Sedov AMR | 0 | 0 | PASS | 47.3 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.8 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.4 结论

M4.4 AMR controller 建立完成：refine→coarsen→balance 与 partition 阶段编排已抽取为 `AMRController` 纯接口，主循环无内联残留。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
