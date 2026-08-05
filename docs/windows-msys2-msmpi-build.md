# Windows/MSYS2/UCRT64/MS-MPI 构建与验证指南

本指南记录 `C:\Lagrangian-AMR` 在 Windows 11、MSYS2 UCRT64、Microsoft MPI 和 p4est 环境下的正式构建流程，以及本次实际遇到的临时文件权限问题。目标是让后续会话先确认环境，再判断构建或回归失败是否来自源码。

完整的 G0～G3 黄金回退流程以 [`golden-gates.md`](golden-gates.md) 为准；本文聚焦工具链、构建和环境排障。不要从本文复制旧的单文件比较命令来替代 G1/G3 runner。

## 1. 正式工具链

项目当前正式构建入口是根目录 `Makefile`，不是 `CMakeLists.txt`。Makefile 使用 C++14，并显式链接 p4est/libsc、zlib、MS-MPI 和 Winsock；最终产物为：

```text
bin/AMR_Solver.exe
```

建议组合：

- Windows 10/11
- MSYS2 UCRT64
- `C:\msys64\ucrt64\bin\g++.exe`
- `C:\msys64\usr\bin\make.exe`
- p4est 安装前缀 `third_party/p4est/build/local`
- Microsoft MPI SDK headers/libs
- Microsoft MPI runtime 和 `mpiexec.exe`
- Python 3、NumPy，用于 `python/compare_vtu.py`

当前 CMake 配置仍与正式入口存在差异（包括 C++ 标准和源文件清单），因此不要用 CMake 构建结果替代 Makefile 的 G0 基线。

## 2. 先确认工作目录和工具

所有命令必须从项目根目录执行。PowerShell：

```powershell
Set-Location C:\Lagrangian-AMR
Test-Path .\Makefile
Test-Path .\param.ini
Test-Path .\python\run_tests.py
$env:PATH = "C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
Get-Command make
Get-Command g++
g++ --version
Get-Command mpiexec.exe
```

MSYS2 shell：

```bash
cd /c/Lagrangian-AMR
export PATH="/c/msys64/usr/bin:/c/msys64/ucrt64/bin:$PATH"
command -v make
command -v g++
g++ --version
command -v mpiexec.exe
```

命中的 `g++` 应来自 UCRT64，而不是另一套 MSYS2 ABI。若出现 `make: command not found`，先修正 PATH；不要修改 Makefile 作为绕过。

## 3. 检查 p4est 和 MS-MPI

Makefile 需要以下 p4est/libsc 头文件和库目录：

```text
third_party/p4est/build/local/include
third_party/p4est/build/local/lib
```

MS-MPI 的 SDK 头文件、链接库和运行时是三个不同检查项：

1. 头文件能否由 Makefile 的 `MSMPI_INC` 找到；
2. `msmpi` 库能否由链接器找到；
3. `mpiexec.exe` 启动的进程能否加载 MS-MPI runtime DLL。

编译链接成功不代表 MPI 运行时已经可用，应继续执行最小的 `mpiexec -n 1` 和 `mpiexec -n 4` 验证。

## 4. 正式构建

```powershell
Set-Location C:\Lagrangian-AMR
$env:PATH = "C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
make clean
if ($LASTEXITCODE -ne 0) { throw "make clean failed" }
make -j8
if ($LASTEXITCODE -ne 0) { throw "make failed" }
Test-Path .\bin\AMR_Solver.exe
```

`make clean` 会删除 `build/` 和 `bin/`。执行前确认这些目录中的内容不是需要保留的用户产物；不要用 `git clean` 或其他 destructive 命令清理整个工作树。

## 5. g++/collect2 临时文件权限故障

本项目实际遇到过：

```text
Cannot create temporary file in C:\Windows\: Permission denied
```

它可能发生在源文件编译阶段，也可能发生在最终链接阶段的 `collect2`/linker 临时文件阶段。常见原因是 `TEMP` 或 `TMP` 被传成 `C:\Windows` 等不可写目录；这不是 p4est 缺库或 C++ 源码语法错误。

先在同一个构建会话中检查：

```powershell
$env:TEMP
$env:TMP
$env:TMPDIR
```

将它们设置到当前用户可写目录后，重新执行清理和构建。目录应先存在，并且当前用户有写权限。不要把某个用户的临时目录硬编码进项目配置。

如果编译阶段仍受临时汇编文件影响，可在一次性诊断构建中给 CXXFLAGS 增加：

```text
-pipe
```

`-pipe` 只改变编译阶段中间数据的传递方式，不能保证链接阶段也不创建临时文件。

如果目标文件已经成功生成，而链接阶段仍报 `collect2` 临时文件权限错误，可在同一会话中尝试：

```text
-fno-use-linker-plugin
```

这是本机兼容性诊断/绕过，不是项目的通用修复；它可能改变链接器插件、LTO 或链接性能行为。本次没有把它永久写入 Makefile。使用该选项后仍必须完成完整回归，不能只凭 exe 生成就判定通过。

## 5.1 本次验证的可复现构建方式

本次实际验证表明，不能只在一个 Git Bash 会话中设置变量，再由另一个 PowerShell、IDE 或后台任务启动 `make`。更稳妥的做法是由同一个原生 PowerShell 进程设置变量并直接启动 `make`，让 MSYS2 `make`、固定路径的 UCRT64 `g++.exe` 和最终 `collect2.exe` 继承同一环境：

```powershell
Set-Location C:\Lagrangian-AMR
$env:TEMP = 'C:\Lagrangian-AMR\.tmp'
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;' + $env:PATH
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null

make clean
make -j1 CXXFLAGS='-O2 -g -Wall -std=c++14 -pipe -fno-use-linker-plugin'
```

本次 `make -j1` 已实际完成编译和最终链接，生成 `bin/AMR_Solver.exe`。单线程构建用于先获得完整、按顺序的诊断输出；确认链接成功后，再按正式要求执行 `make -j8`。如果改用 Git Bash，必须在同一个 Bash 进程中设置变量并启动构建；不能把 Bash 中的 `export` 当作对已经启动或另一个 shell 中的原生链接器有效。



| 症状 | 优先检查 | 处理 |
|---|---|---|
| `make: command not found` | `Get-Command make`、PATH | 加入 `C:\msys64\usr\bin` |
| `g++` 命中错误 ABI | `Get-Command g++`、`g++ --version` | 将 UCRT64 放在 PATH 前部 |
| `没有规则可制作目标 clean` | 当前 cwd、`Test-Path Makefile` | 回到 `C:\Lagrangian-AMR` |
| MPI 头文件缺失 | `MSMPI_INC` | 检查 MS-MPI SDK Include |
| MPI 链接库缺失 | `P4EST_LIB`、MS-MPI lib | 检查 Makefile 的 `-L` 和 SDK 安装 |
| `C:\Windows` 临时文件拒绝访问 | `TEMP/TMP/TMPDIR` | 改为可写目录，必要时使用 `-pipe` |
| 目标文件存在但链接失败 | collect2 临时文件、链接插件 | 先修临时目录，必要时诊断 `-fno-use-linker-plugin` |
| `mpiexec` 启动失败或 DLL 缺失 | MPI runtime PATH/安装 | 区分 SDK、runtime、mpiexec 三层 |
| `.bashrc` 出现 `\377\376export` | 用户 shell 配置编码 | 记录为环境警告；不误判为项目源码错误 |

## 7. 回归验证入口

标准顺序是 G0 → G1 → G3；G2 已退休。完整命令、固定 case 参数、VTU/PVTU 契约、失败分类、summary 记录和参考资产保护见 [`golden-gates.md`](golden-gates.md)。这里仅保留入口和环境注意事项：

```powershell
# 在完成 G0 且用同一解释器验证 NumPy 后
& $py .\python\run_tests.py
if ($LASTEXITCODE -ne 0) { throw 'G1 failed; do not start G3' }
& $py .\python\run_mpi_gates.py
```

两个 runner 都会临时写入算例参数，并在 `finally` 中逐字节恢复 `param.ini`；验证结束必须检查 `serial_golden_summary.json` 或 `mpi_gate_summary.json` 的 `param_restored`。参考文件不得未经批准重生成或覆盖。runner 首个 case 失败后停止，未执行的 case 不能报告为 PASS。

### 7.1 Python 比较器解释器

串行回归的求解器可以成功运行，但 VTU 比较阶段仍依赖运行 `python/compare_vtu.py` 的解释器安装 NumPy。若 MSYS2 UCRT64 Python 能启动但没有 `pip` 或 `numpy`，不要把该环境错误归因于求解器或数值回归；应显式选择另一套已安装 NumPy 的 Python，并用同一解释器启动 runner：

```powershell
$py = 'C:\path\to\python.exe'
& $py -c 'import numpy; print(numpy.__version__)'
& $py .\python\run_tests.py
```

本次验证中，使用已安装 NumPy 的原生 Python 后，Noh Uniform、Sod AMR 和 Sedov AMR 均通过 `1e-12` 比较，且 `param.ini` 恢复为原内容。该结果是当时对应提交的 G1 证据，不代表当前 `main` 的 G3 必然通过；当前提交的状态必须以本次 summary 和退出码为准。比较器环境检查应先于继续执行后续 MPI 门禁。

回归前关闭高频诊断环境变量：

```text
LAGRANGIAN_TRACE_TARGET
LAGRANGIAN_TRACE_REFINE
LAGRANGIAN_VERBOSE_AMR
LAGRANGIAN_TRACE_CHECKSUM
```

这些变量会增加全场遍历、日志或文件 I/O，可能把正常回归显著拖慢并污染输出目录。

## 8. 资产保护

- G3 runner 当前不会清空 `output/`，只确保目录存在；执行 G3 前必须确认旧 `.pvtu` 和 rank piece 不会冒充当前目标末帧。不要删除用户需要保留的输出；优先使用独立测试目录或人工确认后隔离旧产物。
- 不把 `output/` 当作 `reference/`。
- 比较前确认末帧来自当前运行，而不是旧输出残留。
- 不修改 `param.ini` 后离开工作树。
- 不因构建失败删除用户未提交的源码、文档或诊断产物。
- 不提交临时 `.o`、运行输出或本机专用构建产物。
