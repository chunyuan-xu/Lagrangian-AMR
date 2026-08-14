# M6 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- M6 基线：`b87aff9`（M6.4）
- M6 范围：M1（CellKey adapter）、M2（IO 审计）、M6.3（Diagnostics 抽取）、M6.4（旧代码审计）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 里程碑摘要

- **M1**：`MeshAdapter::global_sfc_id` 抽取，Global_SFC_ID 语义不变；
- **M2**：IO 拆分审计——writer 封装/output stamp/config parser 在 `src/io/`，生产 `write_solution` 因静态回调与 `#ifdef` 分支延期迁移；
- **M6.3**：Diagnostics p4est adapter 迁移到 `state_invariant_checker.h`，main.cpp 无本地残留；
- **M6.4**：无死代码可清理，trace 均门控，以审计收口。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.8 s |
| Sod AMR | 0 | 0 | PASS | 58.9 s |
| Sedov AMR | 0 | 0 | PASS | 47.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.0 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M6 结论

M6 Mesh adapter、IO 与 Diagnostics 拆分完成。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
