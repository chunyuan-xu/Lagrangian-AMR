# M5.1 P3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- P3 基线：`d13dfac`（P1）
- M5.1 代码范围：P1（角点边界求解抽取）、P2（等价性核对）、P3（收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## P2：等价性核对（静态审计）

- `corner_solve.h` 与旧 `BoundaryNodeVelocityComputation` 在速度分量/矩阵/边界值/阈值 token 上完全一致（24 项交集，无遗漏无新增）；
- main.cpp 中 `BoundaryNodeVelocityComputation` 仅做委托调用；
- 无其他可抽取纯角点数学残留（角点遍历与矩阵组装含 p4est/owner 逻辑，按计划保留）。

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
| Noh Uniform | 0 | 0 | PASS | 19.5 s |
| Sod AMR | 0 | 0 | PASS | 61.4 s |
| Sedov AMR | 0 | 0 | PASS | 48.0 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.0 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M5.1 结论

M5.1 纯角点数学抽取完成：2×2 边界求解抽取为 `CornerSolve` 纯接口，旧函数保留为委托，等价性核对通过。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
