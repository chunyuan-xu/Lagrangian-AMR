# M3.5 C7 最终收口记录（2026-08-06）

## 收口基线与范围

- 分支：`m35-communication-cleanup`
- C7 基线：`0ca26e3`（C6 全量通信审计）
- M3.5 代码范围：C1～C5 五组全仓库零调用旧路径清理
- C6：静态零残留审计已完成
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- 既有 `-Wall` warnings 保持，无编译/链接错误
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.3 s |
| Sod AMR | 0 | 0 | PASS | 58.1 s |
| Sedov AMR | 0 | 0 | PASS | 46.5 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.9 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.1 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## 最终不变量

- `param.ini` 已逐字节恢复，SHA-256 与基线一致
- reference 文件数：46；工作期间未修改或生成 reference
- 五组候选旧路径无生产源码残留
- 活动 GhostSession API、ghost allocation/exchange/rebuild、generation/validity、callback context、owner-local 写门禁和 remote 读取均保留
- 未将 `.o`、`bin/`、`build/`、输出目录、VTU/PVTU、summary JSON 或调试产物加入 Git
- 用户已有的 `docs/reconstruction.md` 改动未纳入本提交

## M3.5 结论

C7 的 G0、G1、G2 N/A、两次完整 G3 全部完成。M3.5 通信旧路径清理闭合，可以作为当前分支的最终收口提交。
