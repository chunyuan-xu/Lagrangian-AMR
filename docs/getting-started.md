# 从零开始：安装、编译与运行（新用户指南）

本指南面向**第一次从 GitHub 拉取本项目的新用户**。读完本文后，你应能在一台干净的 Windows 机器上完成：

装工具链 → 编译 p4est → 编译求解器 → 串行 / MPI 运行 → 跑黄金门禁（G0/G1/G3）。

维护者版的环境排障细节见 [`windows-msys2-msmpi-build.md`](windows-msys2-msmpi-build.md)，门禁的唯一标准见 [`golden-gates.md`](golden-gates.md)，输出格式契约见 [`vtu-pvtu-contract.md`](vtu-pvtu-contract.md)。

## 0. 依赖总览

| 依赖 | 作用 | 来源 |
|---|---|---|
| Windows 10/11 | 运行环境 | — |
| MSYS2 UCRT64 | g++ / make / cmake / ninja / zlib / MS-MPI 头文件与导入库 / Python+NumPy | msys2.org |
| Microsoft MPI 运行时 | `msmpi.dll` + `mpiexec.exe` + `smpd.exe` | 微软 `msmpisetup.exe` 安装包 |
| p4est 2.8（vendored） | 自适应网格库 | 仓库内 `third_party/p4est`，需自行编译 |
| Python 3 + NumPy | VTU/PVTU 比较器 | MSYS2 或任意已装 NumPy 的 Python |

注意：**MS-MPI 拆成两半**——编译期需要的 `mpi.h` 和 `libmsmpi.dll.a` 来自 MSYS2 的 `mingw-w64-ucrt-x86_64-msmpi` 包；运行期需要的 `msmpi.dll` 和 `mpiexec.exe` 来自微软的运行时安装包。两者都要装，缺一不可。

## 1. 安装 MSYS2 UCRT64

1. 到 <https://www.msys2.org/> 下载并安装 MSYS2，**安装到默认目录 `C:\msys64`**（本项目的 Makefile 硬编码了该路径）。
2. 打开「MSYS2 UCRT64」终端（开始菜单里的 "MSYS2 UCRT64" 快捷方式，不是 "MSYS2 MSYS"）。
3. 更新系统包：

   ```bash
   pacman -Syu
   ```

## 2. 安装所需软件包

在 **UCRT64** 终端中执行：

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  make \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-zlib \
  mingw-w64-ucrt-x86_64-msmpi \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-python \
  mingw-w64-ucrt-x86_64-python-numpy \
  git
```

装完后自检：

```bash
g++ --version
cmake --version
ninja --version
test -f /ucrt64/include/mpi.h && echo "mpi.h OK"
test -f /ucrt64/lib/libmsmpi.dll.a && echo "libmsmpi OK"
python -c 'import numpy; print(numpy.__version__)'
```

若 `mpi.h` 或 `libmsmpi.dll.a` 不存在，说明 `mingw-w64-ucrt-x86_64-msmpi` 没装好；这两个文件是编译期链接 MS-MPI 所必需的。

## 3. 安装 Microsoft MPI 运行时

MSYS2 的 msmpi 包**只提供编译期**的头文件和导入库，**不提供** `mpiexec.exe` 和 `msmpi.dll`。运行 MPI 程序需要微软官方的运行时：

1. 从 Microsoft 下载 **Microsoft MPI（MS-MPI）** 安装包 `msmpisetup.exe`（当前为 10.x，建议选择最新稳定版）。
2. 安装。它会：
   - 把 `msmpi.dll` 放到 `C:\Windows\System32\`；
   - 把 `mpiexec.exe`、`smpd.exe` 放到 `C:\Program Files\Microsoft MPI\Bin\`。
3. 自检：

   ```powershell
   Test-Path 'C:\Windows\System32\msmpi.dll'
   Test-Path 'C:\Program Files\Microsoft MPI\Bin\mpiexec.exe'
   & 'C:\Program Files\Microsoft MPI\Bin\mpiexec.exe' -n 1 hostname
   ```

最后一条命令能打印主机名，说明运行时可用。

> 注：Makefile 里的 `MSMPI_INC` 指向 `C:\Program Files (x86)\Microsoft SDKs\MPI\Include`，这是微软 MPI **SDK** 的安装位置。本机实际没有装 SDK，但编译仍能通过，因为 `-IC:/msys64/ucrt64/include` 提供了 MSYS2 版的 `mpi.h`。新用户**不需要**另装 SDK；只要 `mingw-w64-ucrt-x86_64-msmpi` + 微软运行时即可。

## 4. 克隆仓库

```bash
git clone https://github.com/chunyuan-xu/Lagrangian-AMR.git
cd Lagrangian-AMR
```

仓库只包含 p4est 的**源码**（`third_party/p4est`），**不含**编译产物。`third_party/p4est/build/`、`bin/`、`build/`、`output/` 都是本机生成、已 gitignore，克隆后需要自己编译。

## 5. 编译 p4est（关键一步）

p4est 被 vendored 在 `third_party/p4est`。编译产物安装到 `third_party/p4est/build/local`，求解器 Makefile 从那里取头文件和静态库。

**这一步是本项目最容易踩的坑：必须禁用 zlib 检测，否则 p4est 会输出压缩的 VTU 文件，导致黄金门禁对比失败。**

在 **UCRT64** 终端中执行：

```bash
cd third_party/p4est
cmake -B build -G Ninja \
  -Dmpi=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE
cmake --build build
cmake --install build --prefix build/local
```

- `-Dmpi=ON`：p4est 启用 MPI。
- `-DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE`：**关键**。它阻止 p4est 的 `find_package(ZLIB)` 找到 MSYS2 的 zlib，从而关闭 `P4EST_ENABLE_VTK_COMPRESSION`，保证 VTU 以未压缩格式输出，与仓库里的参考解一致。
- 编译期间 p4est 的依赖 libsc 会通过 ExternalProject 从 GitHub 拉取 zlib-ng 2.0.6（`https://github.com/zlib-ng/zlib-ng.git`）。**这一步需要网络**；若网络受限，见第 10 节排障。

完成后确认：

```bash
test -f third_party/p4est/build/local/lib/libp4est.a && echo "p4est built"
```

> 注意：不要直接先跑 `make` 让它自动编译 p4est——Makefile 内置的 `p4est` 目标**没有**带 `-DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE`，会编译出压缩 VTU 的 p4est。正确顺序是：**先手动按上面命令编译 p4est，再执行 `make`**（`make` 看到 `libp4est.a` 已存在就会跳过）。

## 6. 编译求解器

求解器用根目录 `Makefile` + C++14 构建，硬编码了 `C:\msys64\ucrt64\bin\g++.exe`。在 **PowerShell** 中执行（同时设置好临时目录，避免 GCC 回退到 `C:\Windows` 报权限错误）：

```powershell
Set-Location C:\ai\Lagrangian-AMR   # 换成你实际的克隆目录
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
$env:TEMP = 'C:\ai\Lagrangian-AMR\.tmp'
$env:TMP  = $env:TEMP
$env:TMPDIR = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null

make -j8
if ($LASTEXITCODE -ne 0) { throw 'make failed' }
Test-Path .\bin\AMR_Solver.exe
```

产物为 `bin\AMR_Solver.exe`。若 Makefile 里的 MSYS2 路径不是 `C:\msys64`（例如你装到了别的盘），需要相应调整 Makefile 的 `CXX`、`CPPFLAGS`、`LDFLAGS` 中的路径。

## 7. 串行冒烟测试

先看 `param.ini` 里 `which_case` 的取值（SodCartesian=7、SedovCartesian=1、NohCartesian=4），直接运行求解器：

```powershell
& .\bin\AMR_Solver.exe
```

正常结束后 `output/` 下应出现 `p4est_Lagrangian_*.vtu` 文件。若这一步报找不到 DLL 或 MPI 头，回到第 1～3 节自检。

## 8. MPI 冒烟测试

```powershell
& 'C:\Program Files\Microsoft MPI\Bin\mpiexec.exe' -n 4 .\bin\AMR_Solver.exe
```

正常会生成多 rank 的 `.pvtu` 和对应 piece 文件。若 `mpiexec` 报 DLL 缺失或 `smpd` 相关错误，说明微软 MPI 运行时没装好，回第 3 节。

## 9. 运行黄金门禁 G0/G1/G3

门禁的唯一标准和完整流程见 [`golden-gates.md`](golden-gates.md)。快速入口（先确认 Python 装了 NumPy，并设 `$py`）：

```powershell
$py = 'C:\msys64\ucrt64\bin\python.exe'   # 或任意已装 NumPy 的 Python
& $py -c 'import numpy; print(numpy.__version__)'
& $py .\python\run_tests.py
if ($LASTEXITCODE -ne 0) { throw 'G1 failed' }
& $py .\python\run_mpi_gates.py
```

G0/G1/G3 必须按顺序执行；G2 已退休。两个 runner 结束时都会逐字节恢复 `param.ini`，务必核对 summary 里的 `param_restored: true`。

## 10. 常见问题与已知差异

### 10.1 p4est 的 libsc 拉取 zlib-ng 卡住

p4est 编译时，其依赖 libsc 会通过 ExternalProject 从 `https://github.com/zlib-ng/zlib-ng.git` 拉取 zlib-ng（tag `2.0.6`）。网络受限（例如需要代理）时这一步可能长时间无进展。

处理办法：先手动把 zlib-ng 克隆到 ExternalProject 期望的目录，再继续 `cmake --build build`；或为 git 配置代理后重试。具体目录随 p4est 构建布局而定，可观察 `third_party/p4est/build/SC-prefix/src/SC-build/ZLIB-prefix/` 下的内容判断是否卡在克隆阶段。

### 10.2 编译 p4est 时自测链接报 `undefined reference to adler32`

禁用 zlib 后，p4est 自带的**自测程序**可能因 libsc 内部 zlib 符号缺失而链接失败。这**不影响**求解器：求解器 Makefile 显式链接了 `-lz`（MSYS2 的 zlib），因此能正常编译运行。以 `libp4est.a` 是否生成、求解器能否链接为准，不必纠结 p4est 自测。

### 10.3 `make` 时 GCC 报 `Cannot create temporary file in C:\Windows`

这是 `TEMP/TMP/TMPDIR` 未指向可写目录导致的，与源码无关。在**同一个 PowerShell 进程**里设置好 `TEMP/TMP/TMPDIR` 再启动 `make`（见第 6 节）。不要跨 shell（例如在 Git Bash 里 `export` 后又到 PowerShell 里 `make`）传递这些变量。

### 10.4 跨机器数值差异（浮点 1 ULP）

不同 Windows 版本 / MSYS2 更新可能带来不同的 UCRT 数学库（`pow`/`sin`/`cos` 等），导致同一算例在不同机器上的 float32 输出出现约 **1 个 float32 ULP**（在量级 ~4 的密度场上约 `4.77e-7`）的末位舍入差异。这不是代码 bug，而是运行环境的数学库差异。

因此：

- 黄金门禁比较容差已定为 `1e-6`（2026-08-14 起为正式值，唯一来源 `python/gates_common.py` 的 `GATE_TOLERANCE`）。它远小于任何物理误差、足以抓真实回归，同时能覆盖跨机器 float32 的 1 ULP 舍入。
- 早先的 `1e-12` 在数值上等价于要求逐位一致，跨机器必然失败，故放宽到 `1e-6`。若未来某台新机器在 `1e-6` 下仍失败：先看差异是否仍是 ~1–2 ULP（是则属环境，可提到 `1e-5`）；若差异 ≫1e-6 则是真回归，**不要**靠继续放宽掩盖。

### 10.5 根目录没有 README 与许可证文件

仓库根目录当前没有 `README.md`，GitHub 落地页为空；`docs/README.md` 中声明「本项目基于 MIT License，许可证文件位于仓库根目录」，但根目录尚未提交 `LICENSE` 文件。新用户应直接阅读 `docs/` 下的文档（以 [`docs/README.md`](README.md) 为索引）。
