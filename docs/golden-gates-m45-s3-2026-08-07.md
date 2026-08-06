# M4.5 S3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- S3 基线：`2548587`（S2）
- M4.5 代码范围：S1（8 个零引用函数删除）、S2（2 个 coord 回调增补删除）；S3 收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 零引用审计

- S1/S2 共删除 10 个零引用函数（312 行），删除后 `grep` 计数 0；
- S3 最终扫描确认 main.cpp 中无其他零引用 static 函数（`zero-use: NONE`）。

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
| Noh Uniform | 0 | 0 | PASS | 19.3 s |
| Sod AMR | 0 | 0 | PASS | 58.7 s |
| Sedov AMR | 0 | 0 | PASS | 47.1 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.1 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.5 结论

M4.5 AMR 旧代码瘦身完成：10 个零引用旧函数删除，无残留，unused-function 警告消除。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
