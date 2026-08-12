# M8.1.1a 门禁记录：predict_timestep 回调剥离（2026-08-12）

## 基线与范围

- 分支：main
- M8.1.1a 基线：`371d477`（M8 计划）
- 生产改动：
  - 新建 `src/amr/amr_callbacks.h`：`AMRCallbacks::quadrant_predict_timestep_callback`（自 main.cpp 逐字迁入）；
  - `src/main.cpp`：移除本地回调定义（40 行），`predict_timestep` 包装改用 `AMRCallbacks::` 命名空间回调。
- 未修改：时间步预测公式、`TimestepReduction` 聚合、AMR 编排。
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
| Noh Uniform | 0 | 0 | PASS | 14.8 s |
| Sod AMR | 0 | 0 | PASS | 51.8 s |
| Sedov AMR | 0 | 0 | PASS | 39.8 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程。

### 首次 canonical 运行

- Sod AMR：PASS，29.8 s
- Sedov AMR：FAIL——`NumberOfPoints 21688 vs Ref 21736`（与 DoD `0e581e5` 记录的并行 AMR 非确定性完全一致）
- solver exit 0，compare exit 1；`param_restored`：true

### 第一次复现运行

- Sod AMR：PASS，29.0 s；Sedov AMR：PASS，24.9 s

### 第二次确认运行

- Sod AMR：PASS，28.8 s；Sedov AMR：PASS，24.7 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

## 结论

M8.1.1a 的 G0、G1、G3 全部通过（首次 Sedov 失败为已调查的并行 AMR 非确定性，连续两次完整 G3 通过）。predict_timestep 回调已剥离到 `AMRCallbacks`，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
