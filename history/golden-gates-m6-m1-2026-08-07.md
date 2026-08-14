# M6 M1 门禁记录：CellKey adapter 抽取（2026-08-07）

## 基线与范围

- 分支：main
- M1 基线：`00faae2`（M6 计划）
- 生产改动：
  - 新建 `src/mesh/cell_key.h`：`MeshAdapter::global_sfc_id` 纯函数（等价于 `global_first_quadrant[rank] + local_id`）；
  - `src/main.cpp`：`debug_quadrant_copy_variable_to_array_callback` 的 Global_SFC_ID 生成改用该接口。
- 未修改：`Global_SFC_ID` 语义、VTU 输出字段/精度、时间元数据。
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
| Sod AMR | 0 | 0 | PASS | 59.6 s |
| Sedov AMR | 0 | 0 | PASS | 48.0 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 28.2 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## M1 结论

M1 的 G0、G1、G3 全部通过。Global_SFC_ID 生成已抽取为 `MeshAdapter::global_sfc_id` 纯接口，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
