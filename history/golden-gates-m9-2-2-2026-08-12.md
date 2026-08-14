# M9.2.2 门禁记录：Hydro 编排壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.2.2 基线：`69daf11`（M9.1.2）
- 生产改动：
  - 新建 `src/hydro/hydro_controller.h`：`HydroController` 命名空间，迁入 18 个 hydro 编排壳（`predict_timestep`/`RiemannSolver`/`MatrixAssemble`/`ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes`/`ComputeCornerNodeVelocity`/`ComputeCoordinate`/`UpdateDensity`/`UpdateMomentumEquation`/`ComputeWork`/`UpdateEnergyEquation`/`UpdateEquationOfState`/`AcceptNumericalSolution`/`ComputeCornerAndEdgeForce`/`FluxRelaxedResetZero`/`CalculateHalfTimeVariable`/`CalculateCornerRcpLcpNcp`/`ComputeDivergence`/`ComputeSoundSpeed`）；
  - 补充剥离 2 个 M8 遗漏回调（`quadrant_corner_to_point_matrix_assemble_callback`、`quadrant_hanging_point_matrix_assemble_callback`）到 `HydroCallbacks`；
  - `src/main.cpp` 移除 20 个函数（272 行），调用点路由到 `HydroController::`。
- 修正：G0 多次失败（`HydroPhases::` vs `HydroCallbacks::` 命名空间、缺 `solver/hydro_callbacks.h` include、前置声明），均修复后通过。
- 未修改：Riemann 迭代、矩阵装配、更新公式、MPI 通信时序。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 多次因命名空间/include 缺失失败，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.3 s |
| Sod AMR | 0 | 0 | PASS | 57.3 s |
| Sedov AMR | 0 | 0 | PASS | 45.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.9 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.2.2 的 G0、G1、G3 全部通过。Hydro 编排壳已剥离到 `HydroController`，Riemann 循环与 MPI 通信/分发流程未中断，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
