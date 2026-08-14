# M3.4 B9 黄金门禁记录

- 测试日期：2026-08-05
- 分支：`b6-golden-gates-docs`
- B6 基线：`93121b2` (`refactor: close M3.4 B6 read alias anchor`)
- B9 源码锚点：`d26869e` (`refactor: close M3.4 B9 balance owner anchor`)
- 本分支本次累计源码范围：B7 owner-write、B8a corner gradient、B8b matrix owner、B8c corner velocity、B9 balance owner
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
- 比较容差：`1e-12`

构建和测试均在本分支 worktree 中执行，临时目录为：

```text
C:\Lagrangian-AMR-b6-golden-gates-docs\.tmp
```

## G0

执行了：

```powershell
make clean
make -j8
```

结果：

- `make clean`：PASS
- C++14 正式并行构建：PASS
- 最终链接：PASS
- 产物：`bin/AMR_Solver.exe`

编译器产生多条既有 `-Wall` warning，但没有编译或链接错误；warning 不影响 G0 通过条件。

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
| Noh Uniform | 0 | 0 | PASS | 15.877 s |
| Sod AMR | 0 | 0 | PASS | 72.132 s |
| Sedov AMR | 0 | 0 | PASS | 48.690 s |

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
| Sod AMR | 0 | 0 | PASS | 27.264 s |
| Sedov AMR | 0 | 0 | PASS | 23.050 s |

目标和参考：

- Sod：`output/p4est_Lagrangian_3046.pvtu` vs `reference/par4_sod/p4est_Lagrangian_3046.pvtu`
- Sedov：`output/p4est_Lagrangian_3933.pvtu` vs `reference/par4_sedov/p4est_Lagrangian_3933.pvtu`

## 阶段结论

B9 在本分支达到完整 G0/G1/G3 门禁通过条件：

- G0：PASS
- G1：PASS
- G2：RETIRED
- G3：PASS
- `param.ini`：已恢复
- reference 黄金资产：未修改
- 当前用户工作树：未覆盖

该记录只证明本分支当前累计 B7→B9 源码在上述环境和参考资产下通过 canonical 门禁；不修改原始历史提交，也不将构建产物、运行输出或 summary JSON 作为 Git 版本文件提交。
