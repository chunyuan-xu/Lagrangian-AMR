# M4.4 C1 门禁记录：AMR 编排抽取（2026-08-07）

## 基线与范围

- 分支：main
- C1 基线：`abd7611`（M4.4 计划）
- 生产改动：
  - 新建 `src/amr/amr_controller.h`：`AMRController::execute_amr`，将主循环 AMR 编排（refine→rebuild→coarsen tag→coarsen→balance→destroy）逐字搬入；
  - `src/main.cpp`：主循环 AMR 编排块替换为对 `execute_amr` 的调用；添加 include。
- 未修改：阶段顺序、p4est 参数（recursive/allowed_level/callbackorphans/P4EST_CONNECT_CORNER/FULL）、ghost 生命周期。
- 修正：首次 G0 因 `p4est_extended.h` 缺失失败，补 include 后通过。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次尝试因 `p4est_extended.h` 缺失失败，不作为有效门禁。
- 修正后有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.5 s |
| Sod AMR | 0 | 0 | PASS | 61.3 s |
| Sedov AMR | 0 | 0 | PASS | 48.6 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.6 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.6 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C1 结论

C1 的有效 G0、G1、G3 全部通过。AMR 阶段编排已抽取为 `AMRController`，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
