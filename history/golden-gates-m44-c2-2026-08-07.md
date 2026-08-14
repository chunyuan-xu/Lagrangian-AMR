# M4.4 C2 门禁记录：partition 编排抽取（2026-08-07）

## 基线与范围

- 分支：main
- C2 基线：`50531e6`（C1）
- 生产改动：`src/main.cpp` partition 周期块（`p4est_partition`+invalidate+destroy）替换为 `AMRController::execute_partition` 调用。
- 未修改：partition 时序、allowcoarsening 参数、ghost 生命周期、refine/coarsen/balance 编排。
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
| Noh Uniform | 0 | 0 | PASS | 20.0 s |
| Sod AMR | 0 | 0 | PASS | 61.5 s |
| Sedov AMR | 0 | 0 | PASS | 48.3 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 首次 canonical 运行

- Sod AMR：PASS，32.1 s
- Sedov AMR：FAIL，27.4 s
- solver exit code：`4294967293`（`0xc0000409`）；尾部 `Time step is too small in quad 0`
- runner 按首失败规则停止，未执行 Sedov comparator，未将失败记为 PASS；`param_restored`：true

该失败与 M3.5 C2 首次失败同模式（非确定性 MPI 崩溃），未修改源码、reference 或 runner。

### 第一次复现运行

- Sod AMR：PASS，31.8 s
- Sedov AMR：PASS，28.2 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

### 第二次确认运行

- Sod AMR：PASS，32.3 s
- Sedov AMR：PASS，28.0 s
- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：true

两次后续完整 G3 均实际执行 Sod 和 Sedov 并通过 solver 与 comparator；首次失败作为非确定性环境/运行记录保留。

## C2 结论

C2 的源代码变更是 partition 编排的最小抽取。G0、G1 通过；G3 首次出现非确定性 Sedov solver 崩溃后，未修改代码，连续两次完整重跑均通过。`param.ini` 恢复，reference 未变化，可以创建 C2 focused commit。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
