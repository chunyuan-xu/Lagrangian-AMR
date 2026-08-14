# G0～G3 黄金回退标准操作规程

本文档是 Lagrangian-AMR 重构的唯一黄金回退入口。它把构建环境、Python/NumPy、VTU/PVTU 文件契约、求解器退出状态和参考资产保护组合成一套可重复流程。

适用范围：Windows 10/11、MSYS2 UCRT64、原生 MinGW `g++.exe`、Microsoft MPI、p4est 2.8.5+。当前正式构建入口是根目录 `Makefile`；不要用 CMake 构建结果替代本 SOP 的 G0。

## 1. 门禁定义

| 门禁 | 内容 | 当前状态 |
|---|---|---|
| G0 | 在规定工具链中清理并完成 `bin/AMR_Solver.exe` 的正式构建 | 必须通过 |
| G1 | 串行 Noh Uniform、Sod AMR、Sedov AMR，按固定参数运行并与 `.vtu` 黄金文件以 `1e-6` 比较 | 必须通过 |
| G2 | 旧的特定步数串行/MPI 一致性检查 | 已退休，不再作为当前门禁 |
| G3 | 四进程 Sod AMR、Sedov AMR，按固定参数运行并与 `.pvtu` 黄金文件及其 rank pieces 以 `1e-6` 比较 | 必须通过 |

完整回退只能在 G0、G1、G3 全部通过且 `param.ini` 恢复为运行前字节内容时报告 PASS。首个算例失败后停止，不把后续未执行算例报告为通过。

容差的唯一来源是 `python/gates_common.py` 的 `GATE_TOLERANCE`（当前 `1e-6`）。该值于 2026-08-14 由 `1e-12` 放宽，原因见 [`getting-started.md`](getting-started.md) §10.4：不同机器的 UCRT 数学库（`pow`/`sin`/`cos`）差异带来约 1 float32 ULP（~4.8e-7）的末位舍入，`1e-12` 等价于逐位一致、跨机必失败；`1e-6` 远小于物理误差、足以抓真实回归。改容差只改这一处，不要在各 runner 里散落硬编码。

G2 的退休信息记录在 `python/run_mpi_gates.py` 和 `../history/reconstruction.md`；不要为了“补齐 G2”重新创建旧的阶段性脚本。

## 2. 不可变资产和工作树规则

回退前先确认当前分支和工作树状态。不得为了测试执行 `git checkout`、`git reset`、`git restore`、`git clean`，也不得覆盖用户已有未提交修改。

以下对象在黄金回退中受保护：

- `reference/` 下的串行 `.vtu`、并行 `.pvtu` 及其 piece：只读，不重新生成、覆盖、重命名；
- `param.ini`：runner 可以临时改写，但结束后必须逐字节恢复；
- 用户已有的源码、文档、参数和诊断文件：不因构建或测试失败删除；
- `output/`：当前运行的临时结果，不是参考资产。

历史提交测试必须导出到独立目录，并复制只读参考资产和独立临时目录；不要在当前工作树上 checkout 历史版本。

## 3. 环境预检

所有构建和 runner 命令从项目根目录执行。推荐使用同一个原生 PowerShell 进程设置环境并启动命令，因为从 Git Bash、PowerShell、IDE 或后台任务之间切换时，原生 `g++`/`collect2` 可能继承不同的临时目录。

```powershell
Set-Location C:\Lagrangian-AMR
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH

Test-Path .\Makefile
Test-Path .\param.ini
Test-Path .\python\run_tests.py
Test-Path .\third_party\p4est\build\local\include
Test-Path .\third_party\p4est\build\local\lib
Get-Command make
Get-Command g++
g++ --version
Get-Command mpiexec.exe
mpiexec.exe -help | Select-Object -First 1
```

应确认：

- `make.exe` 来自 `C:\msys64\usr\bin`；
- `g++.exe` 来自 `C:\msys64\ucrt64\bin`，不是 MSYS ABI 的编译器；
- MS-MPI SDK 的头文件/库可被 Makefile 找到；
- MS-MPI runtime 能让 `mpiexec.exe` 启动 `bin/AMR_Solver.exe`；
- p4est 前缀同时存在 `include` 和 `lib`；
- `bin/AMR_Solver.exe` 是本次 G0 产生的程序，而不是旧构建残留。

MS-MPI 要分别检查 SDK headers/libs、runtime DLL 和 `mpiexec.exe`；链接成功不代表 MPI 运行时可用。

### 3.1 NumPy 解释器

`python/compare_vtu.py` 使用 NumPy，不使用 `vtk` Python 包。必须用同一个已安装 NumPy 的解释器启动 runner 和比较器：

```powershell
$py = 'C:\path\to\python.exe'
& $py --version
& $py -c 'import numpy; print(numpy.__version__)'
```

本次已验证的解释器为：

```text
C:\Users\a9ood\AppData\Local\Python\pythoncore-3.14-64\python.exe
Python 3.14.3
NumPy 2.4.3
```

MSYS2 UCRT64 Python 即使能够启动，也可能没有 `pip` 或 NumPy。此时选择另一套已安装 NumPy 的 Python，不要修改源码、Makefile 或把环境错误误判为求解器数值失败。

### 3.2 临时目录

先在同一 PowerShell 进程中使用当前用户可写目录：

```powershell
$env:TEMP = 'C:\Lagrangian-AMR\.tmp'
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
```

若出现 `Cannot create temporary file in C:\Windows\: Permission denied`，优先检查这三个变量。`-pipe` 和 `-fno-use-linker-plugin` 只能作为一次性诊断选项，不能替代修正临时目录，也不能永久写入 Makefile。

本次 B15 与合并后 `main` 的构建复核表明，遇到该错误时还必须区分 **源码差异** 与 **启动编译的 shell 环境差异**：

- B15 门禁是在已经配置好的 PowerShell 进程中执行的。该进程先设置 `TEMP`、`TMP`、`TMPDIR`，再直接启动 `make`；原生 Windows `g++.exe` 能继承变量，并把临时文件写入项目下的可写目录。
- 合并后 `main` 第一次尝试是在 Git Bash 中使用 `TEMP=... TMP=... TMPDIR=... make` 启动。虽然 Git Bash 自身能看到这些变量，但当前 MSYS2/Git Bash 到原生 Windows `g++.exe` 的启动链没有按预期传递或转换变量，GCC 最终回退到 `C:\Windows`。
- 随后改为由同一个 PowerShell 进程设置环境变量并直接启动 `make` 后，`main` 的 G0 clean build 和链接成功；这证明该次错误属于 GCC 临时文件环境问题，不是 B15 分支代码与 `main` 代码不同。

错误示例：

```text
Cannot create temporary file in C:\Windows\: Permission denied
```

遇到该错误时，不得据此判断 B15 owner-write 修复失效，也不得跳过 G0 直接运行 G1/G3。应使用同一个 PowerShell 进程重新执行：

```powershell
Set-Location C:\Lagrangian-AMR
$env:TEMP = 'C:\Lagrangian-AMR\.tmp'
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null

& 'C:\msys64\usr\bin\make.exe' clean
if ($LASTEXITCODE -ne 0) { throw 'make clean failed' }
& 'C:\msys64\usr\bin\make.exe' -j8
if ($LASTEXITCODE -ne 0) { throw 'make failed' }
```

若命令必须从 Git Bash 发起，也应让 PowerShell 自己创建环境并直接承载 `make`，而不是依赖 Git Bash 的前缀赋值。详细复盘、兼容命令和诊断记录见 [`windows-msys2-msmpi-build.md`](windows-msys2-msmpi-build.md) 的“Git Bash 与 PowerShell 的环境边界复盘”。

修复后必须重新执行完整 G0，并记录 shell、`g++` 路径、`TEMP/TMP/TMPDIR` 和退出码；只有 G0 通过后，才可判断后续 G1/G3 是否暴露源码或数值问题。

回归前关闭高频诊断变量：

```powershell
' LAGRANGIAN_TRACE_TARGET',' LAGRANGIAN_TRACE_REFINE',' LAGRANGIAN_VERBOSE_AMR',' LAGRANGIAN_TRACE_CHECKSUM' |
  ForEach-Object { Remove-Item Env:$($_.Trim()) -ErrorAction SilentlyContinue }
```

如果用户在 shell 配置中看到 `\377\376export`，这是 `.bashrc` 编码警告，不要把它当作项目源码错误；本 SOP 不修改用户 shell 配置。

## 4. G0：构建门禁

先用单线程获得有序诊断，再用正式并行构建确认标准入口：

```powershell
Set-Location C:\Lagrangian-AMR
$env:TEMP = 'C:\Lagrangian-AMR\.tmp'
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null

make clean
if ($LASTEXITCODE -ne 0) { throw 'make clean failed' }

make -j1 CXXFLAGS='-O2 -g -Wall -std=c++14 -pipe -fno-use-linker-plugin'
if ($LASTEXITCODE -ne 0) { throw 'diagnostic build failed' }

make -j8
if ($LASTEXITCODE -ne 0) { throw 'formal parallel build failed' }

Test-Path .\bin\AMR_Solver.exe
```

`make clean` 只清理由 Makefile 管理的 `build/` 和 `bin/`；不要以 `git clean` 代替。只有编译、最终链接和 executable 检查都成功，G0 才通过。G0 通过后才能运行 G1。

## 5. G1：串行黄金回退

G1 的正式入口是：

```powershell
& $py .\python\run_tests.py
$g1Exit = $LASTEXITCODE
$g1Exit
Get-Content .\serial_golden_summary.json
```

如果使用当前已验证解释器，可直接写完整路径：

```powershell
& 'C:\Users\a9ood\AppData\Local\Python\pythoncore-3.14-64\python.exe' .\python\run_tests.py
```

runner 按以下顺序执行，并在首项失败后停止：

| 算例 | `which_case` | `end_time` | AMR | level | 参考文件 |
|---|---:|---:|---|---|---|
| Noh Uniform | 4 | 0.6 | false | 5→5 | `reference/Noh_32x32.vtu` |
| Sod AMR | 7 | 0.2 | true | 5→7 | `reference/SodAMR.vtu` |
| Sedov AMR | 1 | 0.5 | true | 5→7 | `reference/SedovAMR.vtu` |

公共黄金参数由 `python/run_tests.py` 的 `GOLDEN_COMMON` 固定：`refine_err=1.0`、`coarsen_error=0.8`、`refine_period=4`、`refine_coarsen_time=0.0001`、`write_interval_step=200000`、`max_time_step=200000`；每个 case 还固定 `refine_coarsen_enum=0`，并将 `write_interval_time` 设为该 case 的结束时间。

每个串行 case 运行前，runner 会清理根目录 `output/`，运行 solver，选择 `p4est_Lagrangian_*_0000.vtu` 的末帧，然后调用：

```text
python/compare_vtu.py --target <当前 output 末帧> --ref <reference 文件> --tol 1e-6
```

G1 需要同时满足：solver 退出码为 `0`、比较器退出码为 `0`、三个 case 都执行且状态为 `PASS`、`serial_golden_summary.json` 的 schema 为 `lagrangian-amr.serial-golden.v1`、`param_restored` 为 `true`。summary 中的 `failure=solver` 和 `failure=comparison` 必须按不同类别记录。

## 6. G2：已退休

G2 原先用于特定步数的串行/MPI 一致性抽查，已于 2026-08-04 退休。当前回退不执行 G2，也不使用 G2 的空数组或历史摘要替代 G1/G3。

## 7. G3：四进程并行黄金回退

只有 G1 通过后才执行 G3。运行前必须确认 `output/` 中不存在会与本次目标末帧混淆的旧 `.pvtu` 或 piece。当前 `python/run_mpi_gates.py` 不清空 `output/`，只确保目录存在；推荐先人工隔离/移走当前测试目录中的旧运行产物，且不得删除用户需要保留的文件。

正式入口：

```powershell
& $py .\python\run_mpi_gates.py
$g3Exit = $LASTEXITCODE
$g3Exit
Get-Content .\mpi_gate_summary.json
```

runner 固定使用：

```text
mpiexec.exe -n 4 bin/AMR_Solver.exe
```

并按顺序执行：

| 算例 | `which_case` | `end_time` | 目标 | 参考 |
|---|---:|---:|---|---|
| Sod AMR | 7 | 0.2 | `output/p4est_Lagrangian_3046.pvtu` | `reference/par4_sod/p4est_Lagrangian_3046.pvtu` |
| Sedov AMR | 1 | 0.5 | `output/p4est_Lagrangian_3933.pvtu` | `reference/par4_sedov/p4est_Lagrangian_3933.pvtu` |

G3 公共参数与 G1 的 AMR 参数一致：`enable_amr=true`、`minus_level=5`、`max_level=7`、`refine_coarsen_enum=0`、`refine_err=1.0`、`coarsen_error=0.8`、`refine_period=4`、`refine_coarsen_time=0.0001`、`write_interval_step=200000`、`max_time_step=200000`；`write_interval_time` 等于 case 结束时间。

每个 case 只有在 solver 退出码为 `0` 且目标 `.pvtu` 存在时才进入比较。比较器会解析 `.pvtu` 并加载每一个 `Piece Source` 指向的 `.vtu`。目标 `.pvtu`、所有 rank piece、参考 `.pvtu` 和参考 pieces 都必须属于同一算例、同一时间步和同一套黄金参数。首个 solver 或比较失败后，Sedov 不执行。

G3 需要同时满足：两个 case 的 solver 退出码和比较器退出码均为 `0`、`mpi_gate_summary.json` 的 schema 为 `lagrangian-amr.mpi-gates.v1`、状态为 `PASS`、`param_restored` 为 `true`。`solver_exit_code` 非零时，不得把比较阶段的结果写成数值通过。

## 8. VTU/PVTU 比较标准

详细 XML、字段、piece 和比较器行为见 [`vtu-pvtu-contract.md`](vtu-pvtu-contract.md)。回退中固定以下规则：

- 串行使用 `.vtu`，并行使用 `.pvtu` 加完整 rank pieces；
- 支持当前比较器实现的 ASCII 和 base64 binary DataArray；
- 默认比较 `density`、`Pressure`，并自动追加 target/reference 两侧都存在的已知可发现字段；
- `--fields` 可用空格或逗号显式指定字段，显式模式只比较所列字段；
- `NumberOfPoints`、`NumberOfCells`、字段 shape 必须一致；
- 缺字段、NaN、Inf、解析失败和 Global_SFC_ID 集合不一致直接失败；
- 当两侧都有 `Global_SFC_ID` 时按 ID 排序后比较，只有一侧有该字段时明确提示未对齐；
- 数值判定为最大绝对差不超过 `1e-6`；比较器成功退出 `0`，失败退出 `1`。

手工显式比较示例：

```powershell
& $py .\python\compare_vtu.py `
  --target .\output\current.vtu `
  --ref .\reference\baseline.vtu `
  --tol 1e-6 `
  --fields density Pressure Temperature
```

`Temperature` 只有在生产 writer、target 和 reference 都输出完全同名字段后才可加入；不得因为内存中存在温度量就把它加入当前默认门禁。

## 9. 失败分类和停止规则

| 现象 | 分类 | 处理 |
|---|---|---|
| `make`/`g++`/头文件/库找不到 | 环境或依赖 | 先修工具链，不改源码绕过 |
| `C:\Windows` 临时文件拒绝访问 | 构建环境 | 修正同一进程的 `TEMP/TMP/TMPDIR` |
| linker/collect2 失败 | 构建/链接 | 保存完整日志，先确认临时目录和 ABI |
| solver 非零退出、abort、MS-MPI `0xc0000409` | 求解器阶段 | 记录 stdout/stderr tail；不要运行或宣称后续 case |
| `NaN`/`Inf` 或负能量保护触发 | 求解器数值阶段 | 记录 rank、step、quad 和诊断环境，区别于 VTU 比较失败 |
| `.pvtu`/piece 缺失、XML 解析失败 | 输出格式/拓扑 | 检查当前运行与旧输出残留 |
| 字段缺失、shape 不一致、ID 不一致 | 比较契约 | 检查 writer、piece 和 reference，不重生成 reference 掩盖问题 |
| 最大绝对差大于 `1e-6` | 数值回归 | 停止，记录字段和最大差值 |

历史版本的 G3 PASS 只能证明历史提交；不能替代当前提交的实测证据。单进程、短时间 smoke、只比较一个字段或被首项失败跳过的 Sedov 都不能报告为完整 G3 PASS。

## 10. 结果记录模板

每次正式回退至少保留以下信息：

```text
commit: <git rev-parse HEAD>
platform: Windows 11
compiler: <g++ --version>
python: <python --version>
numpy: <numpy.__version__>
mpi: <mpiexec.exe path/version>
tolerance: 1e-6
G0: PASS/FAIL
G1: PASS/FAIL; summary=serial_golden_summary.json
G2: RETIRED
G3: PASS/FAIL; summary=mpi_gate_summary.json
param.ini restored: true/false
reference assets modified: no
working tree user changes preserved: yes/no
failure category: <if applicable>
```

报告 PASS 前检查 summary、进程退出码、目标文件、参数恢复和参考资产状态；不要只看终端最后一行。

## 11. 当前历史证据

以下是已隔离目录中的历史 G3 证据，用于回归边界研究，不是当前 `main` 的状态：

- M0 trusted baseline `6171376`：G3 Sod/Sedov PASS；
- M3.4 B3 `d5cabc9`：G3 Sod/Sedov PASS；
- M3.4 B6 `93121b2`：G3 Sod/Sedov PASS；
- M3.4 B10 `b79b9c5`：G3 Sod solver FAIL；
- 当前 `main` `f4e99ff`：此前实测 G1 PASS，但 G3 Sod solver 阶段出现 NaN 并失败，Sedov 未执行。

这些记录说明测试方法能够区分历史基线与当前提交；它们不能授权修改参考文件，也不能把当前失败状态改写为 PASS。
