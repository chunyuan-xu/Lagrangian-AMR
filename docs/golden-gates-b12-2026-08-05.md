# M3.4 B12 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B11 基线提交：`0c4131d`
- B12 历史锚点：`201d2ac` (`refactor: close M3.4 B12 parent PCInfo anchor`)
- B12 变更：`quadrant_set_init_parent_edge_callback` 仅由 parent owner 写入 `ParentBounInfo::PCInfo`；此前 B10/B11 owner guards 与 hanging exchange 保留
- 参考资产：`reference/` 未修改
- 参数文件：运行前后逐字节一致

## G0

- `make clean`：PASS
- `make -j8`：PASS
- `bin/AMR_Solver.exe`：链接成功
- 仅有既有 `-Wall` warning，无编译或链接错误

## G1

入口：`python python/run_tests.py`

- schema：`lagrangian-amr.serial-golden.v1`
- status：`PASS`
- tolerance：`1e-12`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 16.515 s |
| Sod AMR | 0 | 0 | PASS | 64.201 s |
| Sedov AMR | 0 | 0 | PASS | 47.320 s |

## G3

入口：`python python/run_mpi_gates.py`

- schema：`lagrangian-amr.mpi-gates.v1`
- status：`PASS`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 26.723 s |
| Sedov AMR | 0 | 0 | PASS | 24.369 s |

目标：`output/p4est_Lagrangian_3046.pvtu`、`output/p4est_Lagrangian_3933.pvtu`
参考：`reference/par4_sod/p4est_Lagrangian_3046.pvtu`、`reference/par4_sedov/p4est_Lagrangian_3933.pvtu`

## 阶段结论

B12 完整通过 G0/G1/G3；G2 保持 retired。未应用 B13–B15。
