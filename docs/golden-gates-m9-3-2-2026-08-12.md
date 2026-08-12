# M9.3.2 门禁记录：IO 写盘壳剥离（2026-08-12）

## 基线与范围

- 分支：main
- M9.3.2 基线：`68936f9`（M9.2.4）
- 生产改动：
  - `src/io/io_callbacks.h` 追加 `IOCallbacks::write_solution`（145 行，主 VTU/PVTU 写盘器，含 OutputStamp 字段注入）、`p4est_debug_output_vtu`（82 行，调试 checkpoint VTU）、`debug_quadrant_copy_variable_to_array_callback`（40 行，debug 数组拷贝）；
  - `src/main.cpp` 移除 3 个函数（约 263 行），3 处 `write_solution` 调用点路由到 `IOCallbacks::`；
  - `io_callbacks.h` 补 `p4est_vtk.h`、`io/output_stamp.h`、`mesh/cell_key.h`、`<direct.h>`（`_mkdir`）include。
- 未修改：VTU 字段、文件名/时间元数据、PVTU FieldData 注入逻辑、SFC ID 计算。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次因 `_mkdir` 缺 `<direct.h>` include 失败，补充后通过，不作为有效门禁。
- 有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接 `bin/AMR_Solver.exe` 均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 60.5 s |
| Sedov AMR | 0 | 0 | PASS | 47.9 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.9 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.7 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M9.3.2 的 G0、G1、G3 全部通过。VTU/PVTU 写盘与调试输出已剥离到 `IOCallbacks`，输出文件与时间元数据逐字节不变，参数恢复，reference 未变化。main.cpp 从 798 减至 535 行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
