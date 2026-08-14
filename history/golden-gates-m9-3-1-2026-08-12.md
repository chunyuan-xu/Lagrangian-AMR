# M9.3.1 门禁记录：初始化壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.3.1 基线：`6eb0890`（M9.2.2）
- 生产改动：
  - 新建 `src/init/initializer.h`：`Initializer` 命名空间，迁入 `Lagrangian_init_condition`、`get_boundary_from_p4est`（自 main.cpp 迁入，约 115 行）；
  - `src/main.cpp` 移除 2 个本地函数，调用点路由到 `Initializer::`（含 `p4est_balance` 的 p4est 初始化回调签名）；
  - 修正：`initializer.h` 缺 `hydro/hydro_callbacks.h`/`solver/hydro_callbacks.h` include（原依赖 main.cpp 的 include 顺序），补充后独立可编译。
- 未修改：初始化物理条件、边界条件、子格子信息生成逻辑。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因 `p4est_balance` 调用点未路由到 `Initializer::` 失败（编译期错误），修复后通过，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.5 s |
| Sod AMR | 0 | 0 | PASS | 58.5 s |
| Sedov AMR | 0 | 0 | PASS | 45.9 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.9 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.3.1 的 G0、G1、G3 全部通过。初始化壳（`Lagrangian_init_condition`/`get_boundary_from_p4est`）已剥离到 `Initializer`，p4est 初始化回调签名保持，初始化物理量与边界行为不变，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
