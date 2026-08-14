# DoD 闭合：hydro 回调体迁移（2026-08-12）

## 基线与范围

- 分支：main
- 基线：`10534c2`（DoD 配置审计）
- 生产改动：
  - 新建 `src/solver/hydro_callbacks.h`：7 个 per-cell hydro 更新回调体（`quadrant_compute_divergence_callback`/`quadrant_compute_soundspeed_callback`/`quadrant_update_density_callback`/`quadrant_update_momentum_callback`/`quadrant_compute_work_callback`/`quadrant_update_energy_callback`/`quadrant_update_EOS_callback`）迁入 `HydroPhases` 命名空间；
  - `src/main.cpp`：移除 7 个本地回调体（205 行），包装函数引用 `HydroPhases::` 命名空间回调。
- 未修改：各回调数值公式、`advance_single_stage` 阶段顺序、守恒量。

## DoD 满足

「main.cpp 不再承载具体数值算法」完全闭合：hydro 更新回调体（实际流体公式）已迁出 main.cpp。

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
| Noh Uniform | 0 | 0 | PASS | 19.1 s |
| Sod AMR | 0 | 0 | PASS | 57.0 s |
| Sedov AMR | 0 | 0 | PASS | 45.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.5 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

hydro 更新回调体已迁移到 `HydroPhases` 模块，main.cpp 不再承载具体数值算法。G0、G1、G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
