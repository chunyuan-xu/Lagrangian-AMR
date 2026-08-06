# M5.3 D3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- D3 基线：`0cee260`（D1 审计）
- M5.3 范围：D1 确定性策略审计（owner 求解）+ D3 门禁收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## D1 结论回顾

- 共享角点采用 owner 求解策略：组装（`corner_to_point_matrix_assemble`）与求解（`corner_velocity`）仅依赖 p4est corner iterate 确定性 sides 顺序；
- 不依赖 rank、本地 quadid、不稳定累加顺序；
- 每个共享角点恰好由一个 owner 求解。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback（1 rank 一致性基准）

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 58.9 s |
| Sedov AMR | 0 | 0 | PASS | 47.0 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback（4 ranks 一致性验证）

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.6 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.5 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M5.3 结论

M5.3 共享角点确定性策略确认：owner 求解，满足确定性约束。1 rank（G1）与 4 ranks（G3）对同一算例通过 `1e-12` 容差比较，实证共享角点结果跨 rank 一致。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
