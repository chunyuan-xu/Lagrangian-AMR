# M8.3.2 门禁记录：物理诊断探针剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.3.2 基线：`a343af8`（M8.3.1）
- 生产改动：`src/io/io_callbacks.h` 追加 `IOCallbacks::quadrant_total_energy_error_callback`（自 main.cpp 逐字迁入）；`src/main.cpp` 移除本地定义，注册路由到 `IOCallbacks::`。
- 未修改：总能量累加公式、MPI Reduce 求和、守恒日志。
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
| Noh Uniform | 0 | 0 | PASS | 19.5 s |
| Sod AMR | 0 | 0 | PASS | 57.4 s |
| Sedov AMR | 0 | 0 | PASS | 42.2 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.2 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.8 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

M8.3.2 的 G0、G1、G3 全部通过。总能量守恒诊断探针已剥离到 `IOCallbacks`，MPI Reduce 全局总能量求和未受破坏，守恒日志对齐，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
