# M3.4 B15 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B14 基线提交：`54bfa59` (`refactor: close M3.4 B14 ghost metadata gate`)
- B15 变更：在 `quadrant_whether_allowing_coarsening_from_corner_callback` 中，仅允许 owner 写入 `idAllowCoarsening`；ghost corner 不再写入本地 ghost mirror
- 变更目的：消除 coarsening corner callback 对 ghost-backed state 的非 owner 写入，保持 coarsening decision 的 owner authority
- 参考资产：`reference/` 未修改
- 参数文件：运行前后逐字节一致
- G2：retired

## G0

- `make clean`：PASS
- `make -j8`：PASS
- `bin/AMR_Solver.exe`：链接成功
- 无编译或链接错误；仅有既存 `-Wall` warning

## G1

入口：`python python/run_tests.py`

- summary：`serial_golden_summary.json`
- schema：`lagrangian-amr.serial-golden.v1`
- status：`PASS`
- tolerance：`1e-12`
- `param_restored`：`true`
- 固定执行顺序：Noh Uniform → Sod AMR → Sedov AMR

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.951 s |
| Sod AMR | 0 | 0 | PASS | 61.561 s |
| Sedov AMR | 0 | 0 | PASS | 48.323 s |

输出目标：

- `output/p4est_Lagrangian_4112_0000.vtu`
- `output/p4est_Lagrangian_3046_0000.vtu`
- `output/p4est_Lagrangian_3933_0000.vtu`

## G3

入口：`python python/run_mpi_gates.py`

- summary：`mpi_gate_summary.json`
- schema：`lagrangian-amr.mpi-gates.v1`
- status：`PASS`
- `param_restored`：`true`
- 固定四进程执行顺序：Sod AMR → Sedov AMR
- 两个算例均实际执行；没有依赖被跳过的算例报告 PASS

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 30.641 s |
| Sedov AMR | 0 | 0 | PASS | 26.506 s |

## 阶段结论

B15 完整通过 G0/G1/G3；G2 保持 retired。M3.4 B9→B15 的逐阶段黄金门禁已完成，每个通过阶段均有独立提交和详细日志。B15 是当前请求范围内的最后一个里程碑。
