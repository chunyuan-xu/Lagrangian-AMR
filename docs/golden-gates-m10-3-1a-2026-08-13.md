# M10.3.1a 门禁记录：能量归约量迁移 ReductionContext（2026-08-13）

## 基线与范围

- 分支：main
- M10.3.1a 基线：`2a9f6e2`（M10.1.1）
- 生产改动：
  - `src/io/io_callbacks.h`：新增 `IOCallbacks::ReductionContext` 结构体（POD，持有 `total_energy_cur/lag/init`）与 `IOCallbacks::reduction_context()` 访问器（header-only 局部 static，初始 {0,0,0}）；
  - `quadrant_total_energy_error_callback`：累加目标从 `p4est_data->total_energy_*` 改为 `reduction_context()`；
  - `StatTotalEnergyError`：清零/归约/init 记录全部走 `reduction_context()`（`current_step`/`current_time` 仍从 `p4est_data` 读，属 M10.4.1 范围）；
  - `src/defines.h`：`p4est_data_t` 移除 `total_energy_cur/lag/init` 成员及构造器初始化，加注释。
- 范围：本次只迁能量归约量（引用集中在 IOCallbacks）；`current_time/step/dt` 族与 `user_pointer` 强耦合，留待 M10.4.1 `P4estBridge`。
- 未修改：能量守恒阈值、MPI 归约逻辑、`.plt` 输出。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 25.2 s |
| Sod AMR | 0 | 0 | PASS | 85.4 s |
| Sedov AMR | 0 | 0 | PASS | 72.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 37.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 32.8 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M10.3.1a 的 G0、G1、G3 全部通过。全局能量归约量（`total_energy_*`）已从 `p4est_data_t` 迁入 `IOCallbacks::ReductionContext`，MPI 归约与守恒检查行为不变，参数恢复，reference 未变化。`p4est_data_t` 进一步瘦身（删 3 成员 + 3 构造器赋值）。未将 `.o`、输出目录、VTU/PVTU、`.plt`、summary JSON 或调试产物加入提交。
