# M7.5 H1 门禁记录：Hydro 更新阶段接口抽取（2026-08-12）

## 基线与范围

- 分支：main
- M7.5 H1 基线：`9e43571`（M7.5 计划）
- 生产改动：
  - 新建 `src/solver/hydro_phases.h`：`HydroPhases::run_volume_update(p4est_t*, volume_cb)` 纯更新包装；
  - `src/main.cpp`：7 个更新包装函数（`ComputeDivergence`/`ComputeSoundSpeed`/`UpdateDensity`/`UpdateMomentumEquation`/`ComputeWork`/`UpdateEnergyEquation`/`UpdateEquationOfState`）委托调用该接口；添加 include。
- 未修改：各更新回调字段公式、`advance_single_stage` 阶段顺序、守恒量。
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
| Noh Uniform | 0 | 0 | PASS | 19.1 s |
| Sod AMR | 0 | 0 | PASS | 56.3 s |
| Sedov AMR | 0 | 0 | PASS | 45.5 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## H1 结论

H1 的 G0、G1、G3 全部通过。7 个 hydro 更新阶段已委托 `HydroPhases::run_volume_update` 纯接口，main.cpp 承载的数值包装减少，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
