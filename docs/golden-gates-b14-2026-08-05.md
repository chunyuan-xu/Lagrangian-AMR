# M3.4 B14 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B13 基线提交：`3e3d115` (`refactor: close M3.4 B13 parent corner gate`)
- B14 修复：保留 children hanging metadata 的 owner-only 写入保护，并在 `Get_AMR_BDY_info` 完成 children metadata callback 后立即调用 `session.exchange()`；保留函数返回后的既有 exchange
- 失败原因：原 B14 在 children owner 写入 `IsHanging`/`TwoBouns` 后，parent-edge callback 在同一函数内读取远程 child 的旧 ghost snapshot；partition/ghost 重建后的四进程 Sod 在 quad 206 触发负能量 abort
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

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.364 s |
| Sod AMR | 0 | 0 | PASS | 62.334 s |
| Sedov AMR | 0 | 0 | PASS | 50.303 s |

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
- 固定四进程顺序：Sod AMR → Sedov AMR
- 两个算例均实际执行；没有依赖被跳过的算例报告 PASS

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 32.296 s |
| Sedov AMR | 0 | 0 | PASS | 27.052 s |

## 阶段结论

B14 完整通过 G0/G1/G3；G2 保持 retired。B14 修复解决了 children metadata owner 写入到 parent-edge remote read 之间缺失的 ghost publication 边界。B15 尚未应用。
