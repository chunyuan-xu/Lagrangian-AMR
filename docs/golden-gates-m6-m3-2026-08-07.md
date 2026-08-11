# M6 M6.3 门禁记录：Diagnostics adapter 抽取（2026-08-07）

## 基线与范围

- 分支：main
- M6.3 基线：`483d241`（M6.2 审计）
- 生产改动：
  - `src/diagnostics/state_invariant_checker.h`：追加 `Diagnostics::InvariantContext`、`invariant_volume_callback`、`check_state_invariants` inline p4est adapter；
  - `src/main.cpp`：删除本地 `InvariantContext`/`invariant_volume_callback`/`check_state_invariants`（61 行），调用点改用 `Diagnostics::check_state_invariants`。
- 未修改：纯单元校验器 `Diagnostics::check_cell_invariants`、不变量逻辑、abort 行为。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS（仅 EOF 空行提示，非错误）

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 20.0 s |
| Sod AMR | 0 | 0 | PASS | 59.7 s |
| Sedov AMR | 0 | 0 | PASS | 47.8 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.2 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## M6.3 结论

M6.3 的 G0、G1、G3 全部通过。Diagnostics p4est adapter 已抽取到 `state_invariant_checker.h`，main.cpp 无本地残留，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
