# M7.2 门禁记录：Physics 纯函数拆分（2026-08-12）

## 基线与范围

- 分支：main
- M7.2 基线：`102a8b2`（M7 B4 收口）
- 生产改动：
  - 新建 `src/physics/physics_alg.h`：`PhysicalAlg::calculate_divergence`、`volume_variation_time_step`、`courant_time_step`（自 alg.cpp 逐字搬入）；
  - `src/alg.cpp`：三个纯函数体替换为对 `PhysicalAlg::` 的委托调用。
- 修正：首次 G0 因 include 缺失与命名空间拼写（`PhysicsAlg` vs `PhysicalAlg`）失败，统一为 `PhysicalAlg` 后通过。
- 未修改：`InitCondition`/`InitBoundaryCondition` 编排型函数、数值公式、容差。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次两次尝试因 include 缺失与命名空间拼写失败，不作为有效门禁。
- 修正后有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.7 s |
| Sod AMR | 0 | 0 | PASS | 58.9 s |
| Sedov AMR | 0 | 0 | PASS | 47.6 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## M7.2 结论

M7.2 的 G0、G1、G3 全部通过。Physics 纯函数（divergence/volume step/courant step）已拆分为 `PhysicalAlg` 模块，编排型函数保留在 alg.cpp，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
