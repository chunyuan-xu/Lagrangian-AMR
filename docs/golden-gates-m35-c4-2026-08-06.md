# M3.5 C4 门禁记录：删除 post-coarsening 死路径（2026-08-06）

## 基线与范围

- 分支：`m35-communication-cleanup`
- C4 基线：`bc3f346`（C3 focused cleanup）
- 生产改动：仅 `src/main.cpp`
- 原子删除组：`postprocess_after_coarsening` 与 `quadrant_update_after_coarsening_callback`
- 静态审计：两者定义、wrapper、唯一 callback 注册和所有调用均已删除；生产 `src` 无残留
- 活动 coarsening 判据、balance callback、hanging repair、GhostSession refresh/rebuild/exchange 时序未修改
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

第一次不完整删除 wrapper 的构建不计入证据，构建失败后立即补齐原子组并重新执行。

有效 G0：

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
| Noh Uniform | 0 | 0 | PASS | 19.3 s |
| Sod AMR | 0 | 0 | PASS | 61.2 s |
| Sedov AMR | 0 | 0 | PASS | 47.5 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`

固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.9 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C4 结论

C4 的有效 G0、G1、G3 全部通过。第一次不完整删除导致的构建失败未被计入门禁，未修改 reference、参数或 runner。post-coarsening 原子死路径已最小删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
