# M3.5 C6 门禁记录：全量静态审计（2026-08-06）

## 基线与审计范围

- 分支：`m35-communication-cleanup`
- C6 基线：`c5fd76c`（C5 focused cleanup）
- C6 不扩大删除范围，仅验证 C1～C5 五组对象和活动通信路径
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 五组零残留证明

生产 `src` 中以下对象均无定义、声明、调用、注册或函数指针残留：

- `GhostSession::data_size_`
- `quadrant_vtk_coord_update_callback`
- `Predict_refining_Quads`
- `quadrant_predict_refining_quads_callback`
- `postprocess_after_coarsening`
- `quadrant_update_after_coarsening_callback`
- `quadrant_update_parent_velo_press_callback`

C3 和 C4 的 wrapper/callback 均按原子组删除，未留下失效 `p4est_iterate` 注册。

## 活动路径保留证明

静态审计确认以下活动路径仍存在：

- `GhostSession::initialize()`、`destroy()`、`rebuild()`、`exchange()`；
- `get()`、`data()`、`remote()`、`valid_remote_id()`；
- generation、topology version、validity assert；
- `GhostCallbackContext` 及活动 callback context 注册；
- 活动 `p4est_iterate` 注册；
- owner-local 写入门禁，包括 MatrixP/RHS、点 metadata、parent PCInfo/Lcp、children metadata 和 coarsening corner 写入；
- remote read-only 访问及 ghost allocation/exchange/rebuild 生命周期。

C6 未修改活动 API、callback 参数、遍历顺序、exchange 次数、AMR/物理路径或 `corner_solver.h` 兼容声明。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---:|
| Noh Uniform | 0 | 0 | PASS | 20.4 s |
| Sod AMR | 0 | 0 | PASS | 61.5 s |
| Sedov AMR | 0 | 0 | PASS | 46.7 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 27.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.0 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C6 结论

C6 的全量静态审计、G0、G1、G3 全部通过。五组死路径无残留，活动 GhostSession、context、owner-local、remote、exchange/rebuild/generation/validity 和 callback 注册均保留。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
