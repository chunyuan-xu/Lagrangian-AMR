# M5.1 P1 门禁记录：角点边界求解抽取（2026-08-07）

## 基线与范围

- 分支：main
- P1 基线：`8fd7770`（M5.1 计划）
- 生产改动：
  - 新建 `src/physics/corner_solve.h`：`CornerSolve::boundary_node_velocity`，将 `BoundaryNodeVelocityComputation` 逐字搬入；
  - `src/main.cpp`：`BoundaryNodeVelocityComputation`（222 行体）改为委托调用纯接口；添加 include。
- 未修改：所有边界分支公式、阈值（`1e-10`/`1e-12`）、矩阵逆/点乘、角点遍历。
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
| Noh Uniform | 0 | 0 | PASS | 19.0 s |
| Sod AMR | 0 | 0 | PASS | 58.9 s |
| Sedov AMR | 0 | 0 | PASS | 47.4 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## P1 结论

P1 的 G0、G1、G3 全部通过。角点 2×2 边界求解已抽取为 `CornerSolve` 纯接口，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
