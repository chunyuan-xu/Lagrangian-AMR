# M3.5 C3 门禁记录：删除 prediction 死路径（2026-08-06）

## 基线与范围

- 分支：`m35-communication-cleanup`
- C3 基线：`2ffb1ba`（C2 focused cleanup）
- 生产改动：仅 `src/main.cpp`
- 原子删除组：`Predict_refining_Quads` 与 `quadrant_predict_refining_quads_callback`
- 静态审计：两者定义、wrapper、唯一 callback 注册和所有调用均已删除；生产 `src` 无残留
- 活动 refine criterion、默认 refine tag、AMR 判据、拓扑和 ghost rebuild 未修改
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
| Noh Uniform | 0 | 0 | PASS | 18.8 s |
| Sod AMR | 0 | 0 | PASS | 59.6 s |
| Sedov AMR | 0 | 0 | PASS | 45.9 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`

固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 4 | 0 | PASS | 29.9 s |
| Sedov AMR | 4 | 0 | PASS | 26.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C3 结论

C3 的 G0、G1、G3 全部通过。prediction 原子死路径已最小删除，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
