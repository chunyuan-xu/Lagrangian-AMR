# Lagrangian-AMR

Lagrangian-AMR 是一个基于 **p4est** 的二维拉格朗日可压缩流体动力学求解器，支持非一致四边形网格、MPI 并行、自适应网格加密与减疏（AMR），并提供 VTU/PVTU、守恒误差和剖面数据输出。

当前分支采用精简布局，主要保留求解器源码、依赖库、构建入口、运行配置和本 README。旧设计文档、历史门禁、测试脚本与参考结果已经从本分支移除，但仍可从删除前的 Git 历史恢复。

本 README 中保留的基础 AMR 算例包括：

- SedovCartesian：距离激波阵面的环带加密，L4--L6；
- NohCartesian：距离激波阵面的环带加密，L4--L6；
- Sod1DCartesian：基于密度梯度的自适应加密，L4--L6。

> 本文档记录截至 2026 年 9 月 11 日当前分支的使用方式。程序方案、数学方案和 Verification & Validation（V&V）方案将在后续逐步补充。

## 1. 目录结构

```text
Lagrangian-AMR/
├── src/                         # 求解器源码
│   ├── main.cpp                 # 主程序、时间推进与 AMR 流程
│   ├── alg.cpp / alg.h          # 核心数值与几何算法
│   ├── defines.h                # 全局配置、枚举与兼容数据结构
│   └── io/config_parser.*       # param.ini 解析器
├── include/                     # 头文件或外部接口
├── third_party/p4est/           # p4est 源码及本地安装目录
├── param.ini                    # 默认运行参数
├── Makefile                     # 当前分支可用的构建入口
└── README.md                    # 构建、运行、算例与方案入口
```

运行产生的主要文件包括：

```text
output/p4est_Lagrangian_*.pvtu   # ParaView 并行总索引
output/p4est_Lagrangian_*_*.vtu  # 各 MPI 进程的分块结果
output/p4est_Lagrangian_*.visit  # VisIt 时间步/分块索引
EnergyError.plt                  # 能量误差历史
DistanceProfiles.plt             # 距离/半径剖面数据
```

## 2. 环境与依赖

### 2.1 基础依赖

- 支持 C++14 的编译器；
- CMake 3.18 或更高版本；
- Ninja（推荐，也可换用其他 CMake 生成器）；
- MPI C++ 环境；
- p4est；
- libsc；
- zlib；
- Windows 链接时还需要 `ws2_32`。

当前 Windows 开发环境采用：

- MSYS2 UCRT64 GCC；
- Microsoft MPI；
- 仓库内的 `third_party/p4est`；
- p4est 本地安装前缀 `third_party/p4est/build/local`。

## 3. CMake 配置、编译与运行

### 3.1 当前分支的 CMake 状态

当前分支根目录**尚无顶层 `CMakeLists.txt`**，因此现在直接执行：

```powershell
cmake -S . -B build-cmake
```

会因缺少项目级 CMake 配置而失败。下面给出的命令是本项目准备恢复的标准 CMake 工作流，其目标程序名为 `AMR_Solver`，并约定通过 `P4EST_ROOT` 指定 p4est 安装前缀。

在顶层 `CMakeLists.txt` 恢复以前，请使用[第 4 节](#4-当前可用的-makefile-构建方式)的 Makefile 构建方式。不要把第三方目录中的 `CMakeLists.txt` 当作求解器的顶层构建入口。

### 3.2 Windows：MSYS2 UCRT64 + MS-MPI

在 PowerShell 中进入仓库根目录，确保 CMake、Ninja、编译器和 MS-MPI 可被找到：

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
```

顶层 CMake 配置恢复后，可按以下方式配置 Release 构建：

```powershell
cmake -S . -B build-cmake -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DP4EST_ROOT="$PWD/third_party/p4est/build/local"
```

编译：

```powershell
cmake --build build-cmake --parallel 8
```

单进程运行：

```powershell
.\build-cmake\AMR_Solver.exe
```

四进程 MPI 运行：

```powershell
mpiexec -n 4 .\build-cmake\AMR_Solver.exe
```

### 3.3 Linux / HPC

先加载站点提供的编译器、MPI、CMake 和 Ninja 环境，并安装带 MPI 支持的 p4est。不同超算的模块名称不同，例如：

```bash
module load gcc
module load mpi
module load cmake
module load ninja
```

顶层 CMake 配置恢复后，使用实际的 p4est 安装前缀完成配置和编译：

```bash
cmake -S . -B build-cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DP4EST_ROOT=/path/to/p4est/install

cmake --build build-cmake --parallel
```

运行命令应以超算调度系统的要求为准。交互节点上的常见形式为：

```bash
mpirun -np 4 ./build-cmake/AMR_Solver
```

对于 Slurm 作业，通常使用：

```bash
srun -n 4 ./build-cmake/AMR_Solver
```

> 请勿直接在登录节点进行大规模或长时间计算；应提交到计算节点。MPI 启动命令和进程绑定方式应遵循目标集群的说明。

## 4. 当前可用的 Makefile 构建方式

当前分支已经可以通过根目录 `Makefile` 构建。Windows PowerShell 中执行：

```powershell
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
make -j8
```

首次构建时，Makefile 会在缺少本地 p4est 静态库的情况下尝试编译并安装 `third_party/p4est`。成功后生成：

```text
bin/AMR_Solver.exe
```

单进程运行：

```powershell
.\bin\AMR_Solver.exe
```

四进程 MPI 运行：

```powershell
mpiexec -n 4 .\bin\AMR_Solver.exe
```

也可以使用 Makefile 目标：

```powershell
make run
make run-mpi NP=4
```

仅在确有需要时清理求解器构建产物：

```powershell
make clean
```

`make cleanall` 还会删除 p4est 构建目录，随后需要重新编译第三方库，一般不建议用于日常迭代。

## 5. 运行目录约定

求解器固定从**当前运行工作目录**读取 `param.ini`，并把 `output/`、`EnergyError.plt` 和 `DistanceProfiles.plt` 写入当前工作目录。因此最简单的方式是从仓库根目录运行：

```powershell
mpiexec -n 4 .\bin\AMR_Solver.exe
```

为了隔离不同算例，也可以为每次计算建立独立目录，并复制参数文件和可执行文件：

```powershell
New-Item -ItemType Directory -Force .tmp\runs\my-case | Out-Null
Copy-Item .\bin\AMR_Solver.exe .tmp\runs\my-case\
Copy-Item .\param.ini .tmp\runs\my-case\
Set-Location .tmp\runs\my-case
mpiexec -n 4 .\AMR_Solver.exe
```

这样可以避免新计算覆盖上一次的输出。运行前应确认目标目录中的 `param.ini` 是本次需要的版本。

## 6. `param.ini` 关键参数

| 参数 | 含义 |
|---|---|
| `which_case` | 算例名称，例如 `SedovCartesian`、`NohCartesian`、`Sod1DCartesian` |
| `start_time` / `end_time` | 起始和终止物理时间 |
| `delta_time` | 初始/配置时间步参数，实际推进仍受求解器时间步控制 |
| `minus_level` | 最低 AMR 层级，也是初始基础层级 |
| `max_level` | 最高 AMR 层级 |
| `max_time_step` | 最大推进步数，必须足够大才能到达 `end_time` |
| `refine_coarsen_enum` | AMR 判据：`0` 密度梯度，`1` 压力梯度，`2` 密度值，`3` 压力值，`4` 涡量，`5` 距离 |
| `refine_err` | 加密阈值；其作用取决于所选 AMR 判据 |
| `coarsen_error` | 减疏阈值；其作用取决于所选 AMR 判据 |
| `refine_coarsen_time` | 开始执行 AMR 的物理时间 |
| `refine_period` | 每隔多少时间步执行一次 AMR 判断 |
| `distance_shock_radius_scale` | 预设激波轨迹的比例系数 `C` |
| `distance_shock_radius_exponent` | 预设激波轨迹的时间指数 `alpha` |
| `distance_band_half_width` | 激波轨迹两侧加密环带的半宽 |
| `write_interval_time` | 按物理时间控制输出间隔 |
| `write_interval_step` | 按时间步控制输出间隔 |

距离判据采用预设激波轨迹：

```text
R_s(t) = C t^alpha
```

其中 `C = distance_shock_radius_scale`，`alpha = distance_shock_radius_exponent`。`distance_band_half_width` 决定围绕该轨迹保持细网格的空间范围。

> 当前代码只解析本节和下方案例中列出的参数。旧参数文件中可能存在 `enable_amr`、`repartition_period`、`profiletype`、`static_ring_mesh`、`uniform_level_switch` 或 `disable_coarsening` 等键，但当前 `load_from_config()` 不读取它们，不能依靠这些键改变运行行为。当前 repartition 周期仍使用源码默认值 8。

## 7. 基础 AMR 算例配置

下面三个配置均面向当前代码接口。使用时将相应内容写入运行目录中的 `param.ini`，然后从该目录启动求解器。

### 7.1 SedovCartesian：距离 AMR，L4--L6

该算例采用 Sedov 预设激波轨迹：

```text
R_s(t) = sqrt(t)
```

并在激波位置两侧半宽 `0.04` 的环带内加密到 L6，远离激波区域允许减疏到 L4。

```ini
# SedovCartesian：L4-L6，预设激波半径 R_s(t)=sqrt(t)
which_case = SedovCartesian
start_time = 0.0
end_time = 1.0
delta_time = 1e-5
minus_level = 4
max_level = 6
max_time_step = 100000

refine_coarsen_enum = 5
refine_err = 30.0
coarsen_error = 5.0
distance_shock_radius_scale = 1.0
distance_shock_radius_exponent = 0.5
distance_band_half_width = 0.04
refine_coarsen_time = 0.0
refine_period = 1

write_interval_time = 0.05
write_interval_step = 200000
```

说明：

- `refine_coarsen_enum = 5` 选择距离判据；
- 距离分支根据预设激波轨迹和环带半宽作出加密/减疏决策；
- `refine_err` 和 `coarsen_error` 在当前距离分支中不参与判定，此处保留它们用于记录实验配置；
- 该配置已使用 4 个 MPI 进程运行至 `t ≈ 1.000568`，末态约 1462 个叶子单元。

### 7.2 NohCartesian：距离 AMR，L4--L6

该算例采用 Noh 预设激波轨迹：

```text
R_s(t) = t / 3
```

并在激波位置两侧半宽 `0.02` 的环带内加密到 L6。

```ini
# NohCartesian：L4-L6，预设激波半径 R_s(t)=t/3
which_case = NohCartesian
start_time = 0.0
end_time = 0.6
delta_time = 1e-5
minus_level = 4
max_level = 6
max_time_step = 100000

refine_coarsen_enum = 5
refine_err = 1.0
coarsen_error = 0.8
distance_shock_radius_scale = 0.3333333333333333
distance_shock_radius_exponent = 1.0
distance_band_half_width = 0.02
refine_coarsen_time = 0.0
refine_period = 1

write_interval_time = 0.05
write_interval_step = 200000
```

说明：

- `refine_coarsen_enum = 5` 选择距离判据；
- 终止时刻 `t = 0.6` 的理论激波半径为 `0.2`；
- `refine_err` 和 `coarsen_error` 在当前距离分支中不参与判定；
- 该配置已使用 4 个 MPI 进程运行至 `t ≈ 0.600033`，末态约 1117 个叶子单元。

### 7.3 Sod1DCartesian：密度梯度 AMR，L4--L6

Sod1DCartesian 使用二维笛卡尔网格表示一维激波管问题，并根据密度梯度执行 L4--L6 自适应加密。

```ini
# Sod1DCartesian：L4-L6，密度梯度 AMR
which_case = Sod1DCartesian
start_time = 0.0
end_time = 0.2
delta_time = 1e-5
minus_level = 4
max_level = 6
max_time_step = 100000

refine_coarsen_enum = 0
refine_err = 1.0
coarsen_error = 0.8
refine_coarsen_time = 0.0001
refine_period = 4

write_interval_time = 0.02
write_interval_step = 100000
```

说明：

- `refine_coarsen_enum = 0` 选择密度梯度判据；
- AMR 从 `t = 0.0001` 开始，每 4 个时间步判断一次；
- 该配置已由当前代码单进程运行至 `t ≈ 0.20019`；
- 测试中网格由初始 256 个叶子单元变化为末态 1744 个，其中 L4、L5、L6 分别为 112、224、1408 个；
- 测试记录的最大能量相对误差约为 `2.69e-14`。

## 8. 结果查看与后处理

### 8.1 ParaView 场数据

MPI 计算完成后，优先在 ParaView 中打开 `output/` 下的 `.pvtu` 文件。PVTU 会自动关联各进程写出的 `.vtu` 分块，不需要逐个载入 rank 文件。

常见检查项包括：

- 网格层级与粗细网格分布；
- 密度、压力、速度和内能场；
- 激波位置、激波后平台值和峰值；
- 网格质量及粗细网格交界区域；
- 最终物理时间和输出编号是否符合预期。

### 8.2 PLT 数据

- `EnergyError.plt`：用于检查全局能量误差随时间的变化；
- `DistanceProfiles.plt`：当前默认按照半径/距离输出剖面。

对于 Sod1DCartesian，一维对比通常需要沿物理 `x` 坐标提取剖面。当前 `DistanceProfiles.plt` 默认并不等价于严格的 x 向采样，因此建议从 `.pvtu/.vtu` 中按节点或单元的物理 `NodeX` 坐标进行后处理。

多进程计算的可靠分析应优先基于 `.pvtu/.vtu`。在确认根进程输出和数据汇聚逻辑之前，不应默认多进程写出的 `DistanceProfiles.plt` 一定具有完整且无重复的全局顺序。

## 9. 程序方案（待补充）

> 本节预留用于描述程序总体架构、模块职责、数据结构、p4est 接口、AMR 生命周期、粗细网格数据转移、MPI 通信、负载均衡、输入输出和软件测试组织方式。

待补充内容：

- [ ] 程序模块与依赖关系；
- [ ] 叶子单元数据布局与所有权；
- [ ] 时间推进主流程；
- [ ] 加密、平衡、减疏与 repartition 流程；
- [ ] 粗细网格间物理量转移策略；
- [ ] 串行与 MPI 并行执行路径；
- [ ] 输出、重启和结果后处理方案。

## 10. 数学方案（待补充）

> 本节预留用于描述控制方程、空间离散、时间离散、节点求解器、Riemann 问题、人工黏性或耗散机制、守恒性质、粗细网格耦合和 AMR 误差判据。

待补充内容：

- [ ] 拉格朗日形式的控制方程；
- [ ] 单元中心物理量与节点运动学量；
- [ ] 动量方程和总能量/内能更新；
- [ ] 节点速度、角点力与阻抗构造；
- [ ] 非一致网格及悬挂节点约束；
- [ ] AMR 加密、减疏和重映射的数学定义；
- [ ] 稳定性、守恒性和误差来源分析。

## 11. Verification & Validation（V&V）方案（待补充）

> 本节预留用于记录代码验证、数值验证、解析解/参考解对比、网格收敛性、守恒误差、串并行一致性，以及实验或文献数据验证方案。

待补充内容：

- [ ] 单元测试和关键算子测试；
- [ ] 固定网格与 AMR 结果一致性检查；
- [ ] Sedov、Noh、Sod1D 的解析解或参考解对比；
- [ ] 激波位置、峰值、波后平台和误差范数；
- [ ] 网格与时间步收敛性；
- [ ] 全局质量、动量和能量守恒误差；
- [ ] 串行/MPI 结果一致性；
- [ ] 负载均衡、并行效率和计算成本；
- [ ] 黄金门禁及参考资产更新规则。

## 12. 当前限制与后续工作

- [ ] 恢复并验证求解器顶层 `CMakeLists.txt`；
- [ ] 统一 Windows、Linux 与超算环境的 CMake preset 或 toolchain；
- [ ] 将算例参数整理为独立、可版本控制的配置文件；
- [ ] 将 repartition 周期等运行选项正式接入配置解析器；
- [ ] 完善一维 x 向剖面和 MPI 根进程汇总输出；
- [ ] 在需要进入 V&V 阶段时，按需恢复或重建自动化测试和参考结果；
- [ ] 逐步补全程序方案、数学方案和 V&V 方案。

## 13. License

许可证信息待项目维护者补充。
