# M9.4.1 门禁记录：Simulation 编排与 main.cpp 终极瘦身（2026-08-13）

## 基线与范围

- 分支：main
- M9.4.1 基线：`361bdda`（M9.3.3）
- 生产改动：
  - `src/simulation/simulation.h` 迁入 `Simulation::advance_time_step`（123 行，完整 hydro+AMR 时间步 driver），`Simulation::run` 改为直接调用它（不再接收 driver 函数指针）；
  - `src/main.cpp` 移除 `advance_time_step`（约 126 行）与残留空 `namespace IOAlgorithm {}`，`Simulation::run` 调用改为 3 参数；
  - `simulation.h` 补 `defines.h`/`variable.h`/`core/trace.h`/`mesh/ghost_session.h`/`amr/amr_controller.h`/`amr/amr_callbacks.h`/`hydro/hydro_controller.h`/`io/io_callbacks.h`/`io/output_stamp.h`/`diagnostics/state_invariant_checker.h` include。
- 未修改：时间循环阶段顺序、AMR/ghost/输出调度、守恒检查、MPI 时序。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.0 s |
| Sod AMR | 0 | 0 | PASS | 60.5 s |
| Sedov AMR | 0 | 0 | PASS | 47.6 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.4.1 的 G0、G1、G3 全部通过。时间步 driver 已剥离到 `Simulation`，main.cpp 压至 346 行（启动骨架 + `Simulation::run` 编排）。参数恢复，reference 未变化。**M9 全部子里程碑（M9.1.3/1.4/2.3/2.4/3.2/3.3/4.1）已收口**，main.cpp 从 M8 的 2455 行减至 346 行，实现"压至数百行、仅留启动骨架"目标。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
