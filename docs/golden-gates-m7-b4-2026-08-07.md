# M7 B4 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- B4 基线：`310da00`（B3）
- M7 代码范围：B1（Vec2/Mat2 别名）、B2（构建一致性审计）、B3（最终主程序审计）、B4（收口）
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 里程碑摘要

- **B1**：`Vec2`/`Mat2` 兼容别名，零行为变化；
- **B2**：Makefile 编译 main/alg/config_parser 于 C++14，p4est 独立 CMake 库，无冲突；
- **B3**：主程序初始化→配置→网格→时间循环→收尾结构清晰，模块分工完整。

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
| Sod AMR | 0 | 0 | PASS | 59.0 s |
| Sedov AMR | 0 | 0 | PASS | 48.0 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 32.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.4 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.8 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.2 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M7 结论

M7 基础数学、构建系统与最终主程序审计完成。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
