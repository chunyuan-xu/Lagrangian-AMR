# M7.5 H3 最终收口记录（2026-08-12）

## 收口基线与范围

- 分支：main
- H3 基线：`d9f47f0`（H1）
- M7.5 代码范围：H1（7 个 hydro 更新阶段委托）、H2（等价性核对）、H3（收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## H2：等价性核对（静态审计）

- `hydro_phases.h::run_volume_update` 保持 `p4est_iterate` + `P4_TO_P8` 守卫，与旧包装逐参数一致；
- main.cpp 中 7 个更新包装均委托 `HydroPhases::run_volume_update`；
- 各更新回调字段公式未变。

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
| Noh Uniform | 0 | 0 | PASS | 18.8 s |
| Sod AMR | 0 | 0 | PASS | 56.6 s |
| Sedov AMR | 0 | 0 | PASS | 46.1 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 首次 canonical 运行

- Sod AMR：PASS，29.4 s
- Sedov AMR：FAIL——`NumberOfPoints`/`NumberOfCells` 微小差异（Target 21688 vs Ref 21736）
- solver exit code 0，compare exit 1；`param_restored`：true
- 该失败为非确定性并行 AMR 拓扑差异（与 M3.5 C2/M4.4 C2 同模式），未修改源码/reference/runner。

### 第一次复现运行

- Sod AMR：PASS，30.1 s
- Sedov AMR：PASS，25.6 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

### 第二次确认运行

- Sod AMR：PASS，29.5 s
- Sedov AMR：PASS，26.2 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

两次后续完整 G3 均实际执行 Sod/Sedov 并通过 solver 与 comparator。

## M7.5 结论

M7.5 hydro 更新阶段模块化完成：7 个更新阶段已委托 `HydroPhases`，DoD「main.cpp 不承载数值算法」进一步闭合。G0、G1、连续两次完整 G3 通过（首次非确定性差异作为环境记录保留），参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
