# M3.4 B13 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B12 基线提交：`17a4e1d`
- B13 历史锚点：`c2301fe` (`refactor: close M3.4 B13 parent corner Lcp anchor`)
- B13 变更：parent edge 的 `Lcp` 几何更新仅由 parent owner 执行；此前 B10–B12 owner guards 与 exchange 保留
- 参考资产：`reference/` 未修改
- 参数文件：运行前后逐字节一致

## G0

- `make clean`：PASS
- `make -j8`：PASS
- `bin/AMR_Solver.exe`：链接成功
- 无编译或链接错误，只有既有 `-Wall` warning

## G1

入口：`python python/run_tests.py`

- schema：`lagrangian-amr.serial-golden.v1`
- status：`PASS`
- tolerance：`1e-12`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 17.016 s |
| Sod AMR | 0 | 0 | PASS | 72.148 s |
| Sedov AMR | 0 | 0 | PASS | 49.638 s |

## G3

入口：`python python/run_mpi_gates.py`

- schema：`lagrangian-amr.mpi-gates.v1`
- status：`PASS`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 25.083 s |
| Sedov AMR | 0 | 0 | PASS | 22.535 s |

目标：`output/p4est_Lagrangian_3046.pvtu`、`output/p4est_Lagrangian_3933.pvtu`
参考：`reference/par4_sod/p4est_Lagrangian_3046.pvtu`、`reference/par4_sedov/p4est_Lagrangian_3933.pvtu`

## 阶段结论

B13 完整通过 G0/G1/G3；G2 保持 retired。未应用 B14–B15。
