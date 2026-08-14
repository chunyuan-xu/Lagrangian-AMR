# M5.5 S2 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- S2 基线：`dd71bc7`（S1）
- M5.5 代码范围：S1（`BoundaryNodeVelocityComputation` 委托移除）、S2（残留审计与收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 残留审计

- 删除 `BoundaryNodeVelocityComputation` 后零残留；
- main.cpp 无其他零引用 static 函数（`zero-use: NONE`）；
- `CornerSolve::boundary_node_velocity` 被 `quadrant_corner_velocity_callback` 直接调用；
- `RiemannPhases` 已被 `RiemannSolver` 使用；活动 hydro 更新回调保留。

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
| Noh Uniform | 0 | 0 | PASS | 20.2 s |
| Sod AMR | 0 | 0 | PASS | 61.4 s |
| Sedov AMR | 0 | 0 | PASS | 48.4 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M5.5 结论

M5.5 Hydro 旧代码瘦身完成：`BoundaryNodeVelocityComputation` 委托移除，M5.x 覆盖的旧实现清理干净，无残留。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
