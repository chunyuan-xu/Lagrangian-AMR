# M5.2 R3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- R3 基线：`0dffd43`（R1）
- M5.2 代码范围：R1（Riemann 阶段序列抽取）、R2（阶段锚点审计）、R3（收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## R2：阶段锚点审计（静态审计）

- `RiemannPhases::run_iteration` 保持 3 次 exchange（assemble 后、solve master 后、solve hanging 后）；
- 阶段顺序（assemble→exchange→solve master→exchange→solve hanging→exchange）与原迭代逐字一致；
- 迭代次数（`fixed_iter_num`）与 force 阶段保留在 `RiemannSolver`。

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
| Noh Uniform | 0 | 0 | PASS | 19.0 s |
| Sod AMR | 0 | 0 | PASS | 58.3 s |
| Sedov AMR | 0 | 0 | PASS | 46.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.9 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M5.2 结论

M5.2 Riemann 调用链 phase 化完成：阶段链显式抽取为 `RiemannPhases`，exchange 边界与迭代次数不变。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
