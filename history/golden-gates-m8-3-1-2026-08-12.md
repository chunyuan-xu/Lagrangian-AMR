# M8.3.1 门禁记录：IO 文件输出回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.3.1 基线：`3c7bb2b`（M8.2.2）
- 生产改动：
  - 新建 `src/io/io_callbacks.h`：`IOCallbacks::quadrant_copy_variable_to_array_callback`、`quadrant_write_distance_profiles_callback`、`convert_user_define_index_to_which_corner`（自 main.cpp 逐字迁入）；
  - `src/main.cpp`：移除 3 个本地函数（84 行），注册与索引辅助调用点路由到 `IOCallbacks::`；
  - 修正：`debug_quadrant_copy_variable_to_array_callback` 被误路由，已恢复。
- 未修改：VTU/PVTU 输出字段/精度/命名、distance profiles 写入、时间元数据。
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
| Noh Uniform | 0 | 0 | PASS | 18.3 s |
| Sod AMR | 0 | 0 | PASS | 65.1 s |
| Sedov AMR | 0 | 0 | PASS | 49.7 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

### 首次 canonical 运行

- Sod AMR：PASS，27.2 s
- Sedov AMR：FAIL——`NumberOfPoints 21688 vs Ref 21736`（已知并行 AMR 非确定性）
- solver exit 0，compare exit 1；`param_restored`：true

### 第一次复现运行

- Sod AMR：PASS，25.6 s；Sedov AMR：PASS，25.2 s

### 第二次确认运行

- Sod AMR：PASS，32.0 s；Sedov AMR：PASS，26.9 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

## 结论

M8.3.1 的 G0、G1、G3 全部通过（首次 Sedov 失败为已调查的并行 AMR 非确定性，连续两次完整 G3 通过）。IO 文件输出回调已剥离到 `IOCallbacks`，VTU/PVTU 输出契约不变，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
