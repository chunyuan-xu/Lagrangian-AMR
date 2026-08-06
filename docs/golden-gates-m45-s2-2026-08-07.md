# M4.5 S2 门禁记录：引用残留审计与增补删除（2026-08-07）

## 基线与范围

- 分支：main
- S2 基线：`0122830`（S1）
- 生产改动：仅 `src/main.cpp`，删除 2 个零引用 coord 复制回调（共 40 行）：
  - `quadrant_copy_coordx_to_array_callback`
  - `quadrant_copy_coordy_to_array_callback`
- 审计：S1 后重新扫描 static 函数，发现这 2 个零引用残留，作为 S2 增补删除。
- 删除后零残留（`grep` 计数 0）。
- 未修改：活动回调、AMRController/Policy/Transfer、主循环编排。
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
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 59.3 s |
| Sedov AMR | 0 | 0 | PASS | 47.1 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## S2 结论

S2 的 G0、G1、G3 全部通过。2 个零引用 coord 复制回调已删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
