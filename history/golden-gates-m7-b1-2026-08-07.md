# M7 B1 门禁记录：数学类型兼容别名（2026-08-07）

## 基线与范围

- 分支：main
- B1 基线：`a8a7d66`（M7 计划）
- 生产改动：`src/core/vector_matrix.h` 追加 `using Vec2 = CDoubleVector;` 与 `using Mat2 = CDoubleMatrix;`。
- 未修改：`CDoubleVector`/`CDoubleMatrix` 定义、字段布局、运算符、数值容差。
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

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.7 s |
| Sod AMR | 0 | 0 | PASS | 61.3 s |
| Sedov AMR | 0 | 0 | PASS | 47.3 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.5 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 28.2 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## B1 结论

B1 的 G0、G1、G3 全部通过。`Vec2`/`Mat2` 兼容别名已添加，零行为变化，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
