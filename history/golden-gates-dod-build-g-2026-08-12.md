# DoD 构建唯一化门禁记录（2026-08-12）

## 基线与范围

- 分支：main
- 基线：`6ceec2f`（DoD 构建唯一化提交）
- 改动：移除顶层 `CMakeLists.txt`，Makefile 为唯一正式入口（门禁实际依赖）。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 56.5 s |
| Sedov AMR | 0 | 0 | PASS | 45.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 28.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## 结论

DoD「构建系统唯一」已闭合：Makefile 为唯一正式入口，移除顶层 CMakeLists 后 G0/G1/连续两次 G3 均通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
