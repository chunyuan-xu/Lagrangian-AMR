# M5.5 S1 门禁记录：BoundaryNodeVelocityComputation 委托移除（2026-08-07）

## 基线与范围

- 分支：main
- S1 基线：`a038dda`（M5.5 计划）
- 生产改动：仅 `src/main.cpp`
  - 删除 `BoundaryNodeVelocityComputation` 委托函数（M5.1 后仅剩委托）；
  - 调用点（`quadrant_corner_velocity_callback`）改用 `CornerSolve::boundary_node_velocity` 直接调用。
- 删除后 `BoundaryNodeVelocityComputation` 零残留。
- 未修改：`CornerSolve` 接口、边界分支公式、角点求解逻辑。
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
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 64.4 s |
| Sedov AMR | 0 | 0 | PASS | 48.4 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.1 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.7 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## S1 结论

S1 的 G0、G1、G3 全部通过。`BoundaryNodeVelocityComputation` 委托已删除，调用点直接用 `CornerSolve` 接口，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
