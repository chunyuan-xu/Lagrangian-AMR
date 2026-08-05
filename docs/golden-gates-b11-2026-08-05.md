# M3.4 B11 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B10 基线提交：`d22c333`
- B11 历史锚点：`f91282a` (`refactor: close M3.4 B11 hanging metadata anchor`)
- B11 变更：hanging metadata（`IsHanging`、`TwoBouns`、`BounParent`、master/hanging 坐标）仅写入 owner-local child；B10 的 hanging `MatrixP/RHS` owner guard 与 phase 内 exchange 保留
- 参考资产：`reference/` 未修改
- 参数文件：运行前后逐字节一致

## G0

- `make clean`：PASS
- `make -j8`：PASS
- 最终产物：`bin/AMR_Solver.exe`
- 编译器仅报告既有 `-Wall` warning，无编译或链接错误

## G1 串行黄金回退

入口：`python python/run_tests.py`

- schema：`lagrangian-amr.serial-golden.v1`
- status：`PASS`
- tolerance：`1e-12`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 16.452 s |
| Sod AMR | 0 | 0 | PASS | 61.869 s |
| Sedov AMR | 0 | 0 | PASS | 48.892 s |

目标末帧：`output\\p4est_Lagrangian_4112_0000.vtu`、`output\\p4est_Lagrangian_3046_0000.vtu`、`output\\p4est_Lagrangian_3933_0000.vtu`

## G3 四进程并行黄金回退

入口：`python python/run_mpi_gates.py`

- schema：`lagrangian-amr.mpi-gates.v1`
- status：`PASS`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 28.107 s |
| Sedov AMR | 0 | 0 | PASS | 23.227 s |

目标：`output/p4est_Lagrangian_3046.pvtu`、`output/p4est_Lagrangian_3933.pvtu`
参考：`reference/par4_sod/p4est_Lagrangian_3046.pvtu`、`reference/par4_sedov/p4est_Lagrangian_3933.pvtu`

## 阶段结论

B11 完整通过 G0/G1/G3；G2 保持 retired。未应用 B12–B15。
