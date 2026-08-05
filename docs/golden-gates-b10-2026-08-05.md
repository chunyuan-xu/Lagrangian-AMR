# M3.4 B10 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B9 基线提交：`b9828d7` (`refactor: close M3.4 B9 with golden gates`)
- B10 历史源码锚点：`b79b9c5` (`refactor: close M3.4 B10 hanging matrix owner anchor`)
- B10 本次修复：在 hanging `MatrixP/RHS` owner-only 写入完成后、下游 hanging solver 读取前增加一次 `GhostSession::exchange()`
- 参考资产：当前仓库 `reference/`，未修改
- 参数文件：运行前后逐字节一致

## 环境

- Windows 11
- MSYS2 UCRT64
- `C:\msys64\ucrt64\bin\g++.exe`
- `C:\msys64\usr\bin\make.exe`
- Microsoft MPI `mpiexec.exe`
- p4est 前缀：`third_party/p4est/build/local`
- Python：`C:\Users\a9ood\AppData\Local\Python\pythoncore-3.14-64\python.exe`
- NumPy：2.4.3
- C++：C++14
- 比较容差：`1e-12`
- 构建和测试临时目录：`C:\Lagrangian-AMR-b6-golden-gates-docs\.tmp`

## B10 失败诊断

原始 B10 只对 hanging callback 的 `MatrixP/RHS` 写入增加了 owner-local guard：

```cpp
if (!side[i]->is.hanging.is_ghost[0]) {
    child0.MatrixP = MatrixP;
    child0.RHS = RHS;
}
if (!side[i]->is.hanging.is_ghost[1]) {
    child1.MatrixP = MatrixP;
    child1.RHS = RHS;
}
```

该 guard 阻止了把 ghost mirror 当作权威状态写入，但原调用顺序随后直接进入 `quadrant_relaxed_hanging_solver_callback`。当 hanging child 位于远端时，该 solver 读取的仍是交换前的旧 ghost `MatrixP/RHS` 快照。于是 owner-only 写入虽然正确，却没有在该 phase 的消费者读取前发布；MPI Sod 在 AMR 重分区后出现累计能量漂移，最终触发能量守恒 abort（`0xc0000409`）。串行运行不暴露该问题，因为没有远端 ghost 快照。

修复只补齐通信阶段边界：完成 `quadrant_hanging_point_matrix_assemble_callback` 后立即执行一次 `session.exchange()`，再运行 hanging solver。没有引入 B11/B12 的 metadata 或 parent-edge 写入改动，也没有修改 reference 黄金资产。

## G0 构建

执行：

```powershell
$env:TEMP="C:\Lagrangian-AMR-b6-golden-gates-docs\.tmp"
$env:TMP="C:\Lagrangian-AMR-b6-golden-gates-docs\.tmp"
$env:TMPDIR="C:\Lagrangian-AMR-b6-golden-gates-docs\.tmp"
make clean
make -j8
```

结果：

- `make clean`：PASS
- C++14 并行构建：PASS
- 最终链接：PASS
- 产物：`bin/AMR_Solver.exe`
- 编译器输出既有 `-Wall` warning，但无编译或链接错误

## G1 串行黄金回退

入口：

```powershell
python python/run_tests.py
```

机器摘要：`serial_golden_summary.json`

- schema：`lagrangian-amr.serial-golden.v1`
- status：`PASS`
- tolerance：`1e-12`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 14.656 s |
| Sod AMR | 0 | 0 | PASS | 63.375 s |
| Sedov AMR | 0 | 0 | PASS | 45.728 s |

目标末帧：

- `output\\p4est_Lagrangian_4112_0000.vtu`
- `output\\p4est_Lagrangian_3046_0000.vtu`
- `output\\p4est_Lagrangian_3933_0000.vtu`

## G3 四进程并行黄金回退

入口：

```powershell
python python/run_mpi_gates.py
```

执行器：`mpiexec.exe -n 4 bin/AMR_Solver.exe`

机器摘要：`mpi_gate_summary.json`

- schema：`lagrangian-amr.mpi-gates.v1`
- status：`PASS`
- `param_restored`：`true`

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Sod AMR | 0 | 0 | PASS | 27.989 s |
| Sedov AMR | 0 | 0 | PASS | 22.975 s |

目标和参考：

- Sod：`output/p4est_Lagrangian_3046.pvtu` vs `reference/par4_sod/p4est_Lagrangian_3046.pvtu`
- Sedov：`output/p4est_Lagrangian_3933.pvtu` vs `reference/par4_sedov/p4est_Lagrangian_3933.pvtu`

此前 B10 G3 首项 Sod 失败时，solver 在 AMR 重分区后的能量误差达到约 `-1.09e-6` 并 abort，Sedov 按首项失败规则未执行。本次修复后 Sod 与 Sedov 均完整执行，solver 和比较器退出码全部为 0。

## 阶段结论

B10 在本分支达到完整 G0/G1/G3 门禁通过条件：

- G0：PASS
- G1：PASS
- G2：RETIRED
- G3：PASS
- `param.ini`：已恢复
- reference 黄金资产：未修改
- B10 修复停留在 B10 的 hanging matrix publication/exchange 范围
- 未应用 B11、B12、B13、B14 或 B15

该记录只证明当前累计 B7→B9 源码加 B10 修复在上述环境和参考资产下通过 canonical 门禁；构建产物、运行输出和 summary JSON 不作为 Git 版本文件提交。
