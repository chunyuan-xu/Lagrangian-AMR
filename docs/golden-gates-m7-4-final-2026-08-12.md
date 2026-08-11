# M7.4 最终收口记录（2026-08-12）

## 收口基线与范围

- 分支：main
- M7.4 基线：`15ac4af`（S1）
- M7.4 代码范围：S1（Simulation::run 接口）、S2（main.cpp 瘦身审计）、S3（收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## S2：main.cpp 瘦身审计

- `main()` 现为启动骨架：MPI/p4est 初始化 → 配置加载 → 网格构建 → `Simulation::run(..., advance_time_step)` → 收尾；
- 编排逻辑通过 `Simulation::run` 转发，`advance_time_step` 函数体保持字节一致；
- 无残留旧编排。

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
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 57.2 s |
| Sedov AMR | 0 | 0 | PASS | 46.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.4 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M7.4 结论

M7.4 `Simulation::run()` 编排与 main.cpp 瘦身完成。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
