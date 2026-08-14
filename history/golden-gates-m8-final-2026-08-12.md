# M8 最终收口记录（2026-08-12）

## 收口基线与范围

- 分支：main
- M8 基线：`0361216`（M8.4.1b 审计）
- M8 范围：阶段 8.1（AMR 回调剥离）、8.2（Hydro 回调剥离）、8.3（IO/Diagnostics 回调剥离）、8.4（最终闭环）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 里程碑摘要

- **M8.1.1a/1b**：predict_timestep、edge/cell minmod、hanging 几何辅助剥离到 `AMRCallbacks`；`GhostCallbackContext` 共享化；
- **M8.1.2**：coarsening-edge、after-balance、set-init-parent-edge 剥离到 `AMRCallbacks`；
- **M8.2.1**：corner matrix/velocity/copy 剥离到 `HydroCallbacks`；trace 工具共享化；
- **M8.2.2**：halftime/accept/force/parent-edge/flux 剥离到 `HydroCallbacks`；
- **M8.3.1**：VTU copy/distance-profiles 剥离到 `IOCallbacks`；
- **M8.3.2**：total-energy 探针剥离到 `IOCallbacks`；
- **M8.4.1a**：剩余 5 个 hydro 回调剥离，main.cpp `quadrant_` 回调清零；
- **M8.4.1b**：零死代码、无冗余 include 审计。

结果：main.cpp 从约 4700 行瘦身至约 2455 行，22 个 `quadrant_` 回调全部剥离到 `AMRCallbacks`/`HydroCallbacks`/`IOCallbacks`。

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
| Noh Uniform | 0 | 0 | PASS | 15.1 s |
| Sod AMR | 0 | 0 | PASS | 58.9 s |
| Sedov AMR | 0 | 0 | PASS | 45.8 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.5 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.1 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.5 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M8 结论

M8 main.cpp 极限瘦身完成：全部 22 个 `quadrant_` 回调剥离到 AMR/Hydro/IO 模块，G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
