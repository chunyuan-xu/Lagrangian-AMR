# Lagrangian-AMR

Lagrangian-AMR 是基于 p4est 四叉树自适应网格的二维拉格朗日流体力学求解器，支持 Windows/MSYS2 UCRT64 和 Microsoft MPI 并行运行。

> **第一次从 GitHub 拉取本项目？** 请从 [`getting-started.md`](getting-started.md) 开始：它一步步带你装好 MSYS2 UCRT64 与所需软件包、Microsoft MPI 运行时，编译 p4est（含禁用 zlib 检测的关键一步）、编译求解器，并跑通串行 / MPI 与黄金门禁。

## 验证入口

- [`getting-started.md`](getting-started.md)：**新用户从零安装、编译、运行指南**（工具链、MS-MPI 运行时、p4est 编译、串行/MPI 冒烟测试）。
- [`workflow.md`](workflow.md)：**单人双机协作每日操作手册**（开机 pull、存档/正式提交暗号、收工 push、换机交接、出岔子恢复）。
- [`golden-gates.md`](golden-gates.md)：G0～G3 黄金回退唯一标准 SOP。
- [`windows-msys2-msmpi-build.md`](windows-msys2-msmpi-build.md)：Windows/MSYS2 UCRT64/MS-MPI 构建、临时目录和 Python/NumPy 环境排障。
- [`vtu-pvtu-contract.md`](vtu-pvtu-contract.md)：VTU/PVTU XML、piece、字段、对齐和数值比较契约。
- [`sedov-amr-efficiency-case.md`](sedov-amr-efficiency-case.md)：Sedov-AMR 效率收益典型算例（L4～L7、Distance 环带、t=1 串行基线）。
- [`SKILL.md`](SKILL.md)：回归方法论、参数审计和重构工作约束。
- [`refactoring-playbook.md`](refactoring-playbook.md)：重构方法论与铁律（制定新里程碑计划、推进细粒度重构时查阅）。

## 重构历史（history/）

日常理解项目无需阅读，仅排查历史问题时查阅：

- [`reconstruction.md`](../history/reconstruction.md)：重构阶段、回退锚点和当前历史记录。
- [`context.md`](../history/context.md)：重构上下文（会话恢复用）。
- `*implementation_plan.md`（19 份）：各里程碑重构计划。
- `golden-gates-*.md`（96 份）：历次门禁运行记录。

正式脚本位于 `python/`：

- [`../python/run_tests.py`](../python/run_tests.py)：G1 串行 Noh Uniform→Sod AMR→Sedov AMR。
- [`../python/run_mpi_gates.py`](../python/run_mpi_gates.py)：G3 四进程 Sod AMR→Sedov AMR。
- [`../python/compare_vtu.py`](../python/compare_vtu.py)：VTU/PVTU 比较器，依赖 NumPy，不依赖 Python `vtk` 包。

当前 `main` 是否通过 G0～G3，必须以本次构建、runner 退出码和 `serial_golden_summary.json`/`mpi_gate_summary.json` 为准。历史提交的 PASS 不能替代当前提交的实测证据。

## 目录和产物

```text
Lagrangian-AMR/
├── src/                         # C++ 求解器源码
├── python/
│   ├── run_tests.py             # G1 runner
│   ├── run_mpi_gates.py         # G3 runner
│   └── compare_vtu.py           # NumPy VTU/PVTU comparator
├── reference/                   # 只读黄金资产
│   ├── Noh_32x32.vtu
│   ├── SodAMR.vtu
│   ├── SedovAMR.vtu
│   ├── par4_sod/                # 四进程 Sod 的 .pvtu 和 rank pieces
│   └── par4_sedov/              # 四进程 Sedov 的 .pvtu 和 rank pieces
├── output/                      # 当前运行的临时输出，不是 reference
├── bin/AMR_Solver.exe           # Makefile 生成的求解器
├── Makefile                     # 正式构建入口
└── docs/                        # 本目录
```

串行 runner 会在每个算例前清理根目录 `output/`。G3 runner 当前只确保 `output/` 存在，不会自动清除旧的 `.pvtu`/piece；执行 G3 前必须按 [`golden-gates.md`](golden-gates.md) 核验输出隔离和末帧来源。

## 构建和运行

正式构建使用根目录 `Makefile`、C++14、MSYS2 UCRT64 `g++.exe`、p4est/libsc、zlib、MS-MPI 和 Winsock。完整的 PATH、MS-MPI SDK/runtime、p4est、TEMP/TMP/TMPDIR 和 NumPy 检查见 [`windows-msys2-msmpi-build.md`](windows-msys2-msmpi-build.md)。

最小入口：

```powershell
Set-Location C:\Lagrangian-AMR
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
make -j8
```

不要用 CMake 构建结果替代 G0；当前 CMake 配置与正式 Makefile 入口存在差异。

## 黄金回退快速入口

先完成 NumPy 自检并把 `$py` 设置为已安装 NumPy 的 Python：

```powershell
& $py -c 'import numpy; print(numpy.__version__)'
& $py .\python\run_tests.py
if ($LASTEXITCODE -ne 0) { throw 'G1 failed' }
& $py .\python\run_mpi_gates.py
```

G0、G1、G3 必须按顺序执行；G2 已退休。两个 runner 都会在结束时逐字节恢复 `param.ini`，但仍必须检查 summary 中的 `param_restored: true`。首个算例失败后停止，未执行的后续算例不能报告为 PASS。

## VTU/PVTU 比较

当前门禁固定绝对容差 `1e-6`，唯一来源是 `python/gates_common.py` 的 `GATE_TOLERANCE`（2026-08-14 由 `1e-12` 放宽，以吸收跨机器约 1 float32 ULP 的末位舍入差异，见 [`getting-started.md`](getting-started.md) §10.4）。默认比较器字段为 `density` 和 `Pressure`，并会自动追加两侧同时存在的已知附加字段；字段名大小写敏感。需要显式验证扩展字段时：

```powershell
& $py .\python\compare_vtu.py `
  --target .\output\current.vtu `
  --ref .\reference\baseline.vtu `
  --tol 1e-6 `
  --fields density Pressure Temperature
```

比较器检查 XML/PVTU piece、点数、单元数、字段存在性、数组 shape、NaN/Inf 和数值误差；两侧都有 `Global_SFC_ID` 时按 ID 排序对齐。完整行为见 [`vtu-pvtu-contract.md`](vtu-pvtu-contract.md)。

## 资产保护

- 不修改、覆盖或重生成 `reference/`，除非单独批准黄金资产更新。
- 不把 `output/` 当作 reference，也不让旧输出冒充当前末帧。
- 不用 `git clean`、`reset`、`checkout` 或 `restore` 覆盖用户未提交工作。
- 不提交 `.o`、本机临时输出或其他构建产物。
- 当前 `main` 的失败状态必须如实记录，不能用历史基线 PASS 替代。

## 许可

本项目基于 MIT License。许可证文件位于仓库根目录。
