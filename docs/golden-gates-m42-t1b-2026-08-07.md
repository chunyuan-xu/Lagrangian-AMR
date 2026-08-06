# M4.2 T1b 门禁记录：refine 分发接口抽取（2026-08-07）

## 基线与范围

- 分支：main
- T1b 基线：`821e5b6`（T1a）
- 生产改动：
  - `src/amr/amr_transfer.h`：新增 `AMRTransfer::refine_distribute_buffers`，将 refine 分支的 geometry/physical 缓冲分发循环逐字搬入；
  - `src/main.cpp`：refine 分支中 geometry+physical 分发块（约 50 行）替换为对该接口的调用。
- 未修改：缓冲索引映射、数值公式、守恒量、trace 诊断（保留在 adapter）、`p4est_refine/coarsen/balance_ext` 注册、ghost 生命周期。
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
| Noh Uniform | 0 | 0 | PASS | 19.6 s |
| Sod AMR | 0 | 0 | PASS | 60.7 s |
| Sedov AMR | 0 | 0 | PASS | 48.9 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## T1b 结论

T1b 的 G0、G1、G3 全部通过。refine parent→children 的缓冲分发已抽取为 `AMRTransfer` 纯接口，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
