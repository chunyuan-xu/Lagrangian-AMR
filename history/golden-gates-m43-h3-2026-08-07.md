# M4.3 H3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- H3 基线：`6406e74`（H1 审计）
- M4.3 范围：H1 字段对比审计（判定无重复实现、无共享约束核心可合并）+ H3 幂等与门禁收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## H1 结论回顾

- after_balance 与 set_init_parent_edge 写入字段完全不相交（child 角落约束 vs parent PCInfo/Lcp 组装）；
- 不存在可安全合并的共享约束核心；不执行 H2 合并；
- `quadrant_reset_hanging_info_callback` 仅清零标志，不参与约束。

## H3：幂等与收口

- `refresh_after_balance` 幂等断言（`is not idempotent`）为既有运行时检查；
- G1/G3 实证 after_balance 重复调用不改变输出。

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
| Noh Uniform | 0 | 0 | PASS | 19.8 s |
| Sod AMR | 0 | 0 | PASS | 60.6 s |
| Sedov AMR | 0 | 0 | PASS | 48.5 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 28.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.8 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.3 结论

M4.3 以审计收口：经逐字段对比确认 balance/coarsen 两套 hanging 逻辑无重复实现、无共享约束核心，无需合并。幂等性与 G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
