# M3.5 C1 门禁记录：删除 `GhostSession::data_size_`（2026-08-06）

## 基线与范围

- 分支：`m35-communication-cleanup`
- C1 基线：`d22c35c`（M3.5 R0/R1 契约）
- 生产改动：仅 `src/mesh/ghost_session.h`
- 清理内容：删除未被读取的 `data_size_` 私有字段、构造初始化、`initialize()` 赋值和 `destroy()` 复位
- 保留：`data_` 分配/释放、`exchange()`、`generation_`、`topology_version_`、`valid_`、`rebuild()` 和全部活动访问 API
- G2：`N/A — retired since 2026-08-04`
- reference：相对 `bed5156` 的 Git diff 为空，未修改或重新生成
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
| Noh Uniform | 0 | 0 | PASS | 19.104 s |
| Sod AMR | 0 | 0 | PASS | 60.204 s |
| Sedov AMR | 0 | 0 | PASS | 47.863 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行，未发生首失败提前停止

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`

固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.451 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.615 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## C1 结论

C1 的 G0、G1、G3 全部通过，参数恢复，reference 未变化，可以创建 C1 focused commit。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
