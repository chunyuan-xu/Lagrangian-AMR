# M7.4 S1 门禁记录：Simulation::run 编排接口（2026-08-12）

## 基线与范围

- 分支：main
- M7.4 S1 基线：`92e4153`（M7.4 计划）
- 生产改动：
  - 新建 `src/simulation/simulation.h`：`Simulation::run(p4est_t*, double, double, driver_fn)` 高层编排接口；
  - `src/main.cpp`：`main()` 的 `advance_time_step` 调用改为 `Simulation::run(..., advance_time_step)`；添加 include。
- 未修改：`advance_time_step` 函数体、时间循环逻辑、AMR/hydro 编排、守恒量。
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
| Sod AMR | 0 | 0 | PASS | 56.4 s |
| Sedov AMR | 0 | 0 | PASS | 45.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.5 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## M7.4 S1 结论

S1 的 G0、G1、G3 全部通过。`Simulation::run` 高层编排接口已建立，main() 通过它转发 `advance_time_step`，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
