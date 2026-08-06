# M3.5 C2 门禁记录：删除零调用 VTK 坐标 callback（2026-08-06）

## 基线与范围

- 分支：`m35-communication-cleanup`
- C2 基线：`658cd3b`（C1 focused cleanup）
- 生产改动：仅删除 `src/main.cpp` 中全仓库零调用的 `quadrant_vtk_coord_update_callback`
- 静态审计：生产 `src` 中无定义、声明、调用、`p4est_iterate` 注册或函数指针残留
- 活动 VTK writer、字段、文件名、输出顺序、时间元数据和坐标精度未修改
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

入口：`python/run_tests.py`

固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 20.3 s |
| Sod AMR | 0 | 0 | PASS | 57.0 s |
| Sedov AMR | 0 | 0 | PASS | 46.2 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`

固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 首次 canonical 运行

- Sod AMR：PASS，30.5 s
- Sedov AMR：FAIL，27.1 s
- solver exit code：`4294967293`（Windows `0xc0000409`）
- 失败尾部：`Time step is too small in quad 0`，随后 MPI 进程未调用 finalize
- runner 按首失败规则停止，未执行 comparator，也未将 Sedov 标记为 PASS
- `param.ini`：已恢复

该次失败未修改源码、reference 或 runner。失败发生在 solver 内部而非 VTU comparator。

### 第一次复现运行

- Sod AMR：PASS，29.5 s
- Sedov AMR：PASS，25.9 s
- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`

### 第二次确认运行

- Sod AMR：PASS，30.4 s
- Sedov AMR：PASS，25.2 s
- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`

两次后续完整 G3 均实际执行 Sod 和 Sedov，并通过 solver 与 comparator；首次失败作为非确定性环境/运行记录保留，不被改写为 PASS。

## C2 结论

C2 的源代码变更是零调用 callback 的最小删除。G0、G1 通过；G3 首次运行出现非确定性 Sedov solver 崩溃后，未修改代码或放宽门禁，连续两次完整重跑均通过。`param.ini` 恢复，reference 未变化，可以创建 C2 focused commit。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
