# M3.5 C5 门禁记录：删除 parent velocity/pressure 死 callback（2026-08-06）

## 基线与范围

- 分支：`m35-communication-cleanup`
- C5 基线：`e3be3c1`（C4 focused cleanup）
- 生产改动：仅删除 `src/main.cpp` 中 `quadrant_update_parent_velo_press_callback`
- 静态审计：该 callback 在生产树仅有一个静态定义，无调用、注册、函数指针或测试引用
- 活动 parent-edge、Riemann、corner、hanging、pressure、velocity 和 force assembly 路径未修改
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
| Noh Uniform | 0 | 0 | PASS | 19.9 s |
| Sod AMR | 0 | 0 | PASS | 59.7 s |
| Sedov AMR | 0 | 0 | PASS | 47.9 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`

固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.6 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C5 结论

C5 的 G0、G1、G3 全部通过。parent velocity/pressure 零调用 callback 已最小删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
