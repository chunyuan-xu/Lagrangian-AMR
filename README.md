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
| `initial_annulus_mesh` | 设为 `1` 时，在首次物理推进前建立固定环带拓扑，并关闭后续 AMR 与 repartition |
| `initial_annulus_center_x` / `initial_annulus_center_y` | 固定环带圆心的物理坐标 |
| `initial_annulus_inner_radius` / `initial_annulus_outer_radius` | 固定环带的内、外半径 |
| `initial_annulus_level` | 与固定环带相交的单元所达到的目标层级 |
| `max_time_step` | 最大推进步数，必须足够大才能到达 `end_time` |
| `refine_coarsen_enum` | AMR 判据：`0` 密度梯度，`1` 压力梯度，`2` 密度值，`3` 压力值，`4` 涡量，`5` 距离，`6` 无量纲密度梯度 |
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

> 当前代码只解析本节和下方案例中列出的参数。旧参数文件中可能存在 `enable_amr`、`repartition_period`、`profiletype`、`static_ring_mesh`、`uniform_level_switch` 或 `disable_coarsening` 等键，但当前 `load_from_config()` 不读取它们，不能依靠这些键改变运行行为。固定初始环带应使用新的 `initial_annulus_*` 参数；常规动态计算的 repartition 周期仍使用源码默认值 8。

## 7. 基础 AMR 算例配置

下面配置均面向当前代码接口。使用时将相应内容写入运行目录中的 `param.ini`，然后从该目录启动求解器。

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

### 7.4 SodCartesian：密度梯度 AMR，L4--L7

当前代码中的 SodCartesian 是定义在 `[0,1] × [0,1]` 区域上的二维径向 Sod 问题：初始间断以原点为中心，半径为 `r = 0.5`。它不同于沿 x 方向构造的一维 Sod 激波管。

```ini
# SodCartesian：L4-L7，密度梯度 AMR
which_case = SodCartesian
start_time = 0.0
end_time = 0.2
delta_time = 1e-5
minus_level = 4
max_level = 7
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
- 该配置已由当前代码单进程运行至 `t ≈ 0.20019`，共 3046 个时间步；
- 网格由初始 256 个叶子单元变化为末态 4234 个，其中 L4、L5、L6、L7 分别为 103、249、642、3240 个；
- 末态密度和压力均为正且不存在非有限值，测试记录的最大能量相对误差约为 `1.87e-14`。

### 7.5 TaylorGreen：初始固定环带，L5--L7

该配置先建立全域 L5 网格，再在第一次物理推进之前，将与圆心 `(0.5,0.5)`、半径区间 `[0.3,0.5]` 相交的单元递归加密到 L7。为满足 p4est 的 2:1 平衡约束，环带边界附近会自动出现少量 L6 过渡单元。初始拓扑建立后，不再执行加密、减疏或 repartition。

```ini
which_case = TaylorGreen
start_time = 0.0
end_time = 0.6
delta_time = 1e-5
minus_level = 5
max_level = 7
max_time_step = 100000

initial_annulus_mesh = 1
initial_annulus_center_x = 0.5
initial_annulus_center_y = 0.5
initial_annulus_inner_radius = 0.3
initial_annulus_outer_radius = 0.5
initial_annulus_level = 7

# 以下 AMR 参数在 initial_annulus_mesh = 1 时不参与运行期拓扑更新。
refine_coarsen_enum = 6
refine_err = 0.05
coarsen_error = 0.02
refine_coarsen_time = 0.0
refine_period = 4

write_interval_time = 0.05
write_interval_step = 200000
```

2026-09-11 的 4 进程测试运行至 `t ≈ 0.600037`，固定拓扑含 9952 个叶子单元：L5、L6、L7 分别为 328、504、9120 个。日志中只有启动阶段各一次 refine、balance 和 partition，运行期没有 coarsen 或再次 repartition。

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

### 10.1 无量纲密度梯度 AMR 传感器（原型已实现，验证及完善待实施）

为将绝对密度梯度转换为单元尺度上的相对密度变化量，当前独立原型采用

\[
\eta_{\rho,K}
=\frac{h_K\,|\nabla\rho|_K}
{\max\left(|\rho_K|,\rho_{\mathrm{floor}}\right)}.
\]

当前程序中的各项定义为：

- `|∇ρ|_K` 默认使用距离加权最小二乘重构的梯度向量模（`density_gradient_method = 1`）；设置为 `0` 可恢复旧的最大边差商进行对照；
- `ρ_K` 使用叶子单元当前时刻的单元中心密度 `idDensity_cur`；
- `ρ_floor` 暂取程序已有的正数保护量 `m_eps`，只用于避免真空附近除零；
- `h_K = sqrt(A_K)`，`A_K` 是当前拉格朗日物理四边形面积 `idVolume`，因此 `h_K` 是与该单元等面积正方形的边长；它会随拉格朗日网格运动和单元变形而更新，并不是只由 p4est 层级决定的常数。

这里使用当前物理面积而不是 p4est 逻辑层级尺寸，是因为拉格朗日网格会随流场运动和变形。对于近似各向同性的二维四边形，`sqrt(A_K)` 能代表局部分辨率；对于长宽比很大的畸变单元，后续可比较最小高度、方向尺度或基于面法向的 `h_K` 定义。

加密与减疏暂定义为：

\[
\eta_{\rho,K}>\eta_{\mathrm{refine}}
\quad\Longrightarrow\quad\text{加密},
\]

\[
\max_{K\in\mathrm{siblings}}\eta_{\rho,K}
<\eta_{\mathrm{coarsen}}
\quad\Longrightarrow\quad\text{四兄弟整体减疏}.
\]

并要求 `η_coarsen < η_refine` 形成滞回区。代码中已经新增独立判据 `refine_coarsen_enum = 6`，加密函数逐叶子判断，减疏函数则要求同一 family 的四个兄弟单元全部满足减疏条件。无效面积或非有限状态返回无穷大，因而会促使加密并阻止减疏。

实现入口位于 `src/amr/amr_criteria.h`：

| 函数 | 职责 |
|---|---|
| `CellCharacteristicLength` | 从当前叶子单元 `idVolume` 计算 `h = sqrt(A)`，不增加持久存储字段 |
| `DimensionlessDensityGradientIndicator` | 复用 `idCDensityGradient`，计算无量纲指标 |
| `RefineByDimensionlessDensityGradient` | 在最小/最大层级约束下判断加密 |
| `CoarsenByDimensionlessDensityGradient` | 检查四兄弟、减疏许可和阈值，判断整体减疏 |

原有 `RefineErrorEstimate` / `CoarsenErrorEstimate` 在判据编号为 `6` 时调用上述独立函数；编号 `0` 仍是有量纲密度梯度判据。这两个编号都通过 `density_gradient_method` 选择底层梯度算法。对超出最大允许层级的有效单元族，现有层级恢复规则优先于传感器阈值；最小层级、减疏禁止标记和无效状态保护仍然生效。标记允许减疏不保证最终网格一定变粗，仍需满足四兄弟完整性和 2:1 平衡。

启用示例（阈值仅为测试起点，不自动修改根目录配置）：

```ini
enable_amr = true
refine_coarsen_enum = 6
refine_err = 0.20
coarsen_error = 0.08
```

`h` 的直观含义：面积为 `0.01` 时 `h = 0.1`；若均匀细分为四个等面积子单元，各子单元的 `h` 减半。只有未变形的单位正方形树上的均匀网格才有 `h = 2^(-LEVEL)`；物理区域缩放和拉格朗日变形后应以实际面积为准。当前实现针对二维笛卡尔面积解释，不能未经核验直接推广到带径向权重的体积或三维网格。

待实施/验证：

- [ ] 标定不同算例的阈值、AMR 调用周期及误差，避免将无量纲误解为不依赖分辨率。
- [ ] 验证低密度平滑区的相对梯度响应；该指标不是仅在激波处触发的探测器，也不保证波前/波后自动减疏。
- [ ] 将 `ρ_floor` 从通用数值保护量中独立为可配置密度尺度；当前 `m_eps = 1e-12` 不能消除低密度区域的高相对梯度。
- [ ] 对比畸变网格的方向尺度，并核验边界、粗细接口及串并行梯度一致性。

2026-09-11 的首轮标定结果如下，使用的是旧最大边差商（现在需显式设置 `density_gradient_method = 0`），不是新的最小二乘结果。网格占比以对应的全最高层均匀网格为分母；这些阈值是实验起点，不应视为跨问题通用常数。

| 算例 | 层级 | `η_refine` | `η_coarsen` | 末态叶子数 | 最高层均匀网格 | 网格占比 |
|---|---:|---:|---:|---:|---:|---:|
| SedovCartesian | L4--L6 | 0.20 | 0.08 | 1732 | 4096 | 42.3% |
| NohCartesian | L4--L6 | 0.20 | 0.08 | 1267 | 4096 | 30.9% |
| SodCartesian | L4--L7 | 0.05 | 0.02 | 4879 | 16384 | 29.8% |
| TwoDimRiemann | L4--L6 | 0.15 | 0.06 | 1993 | 4096 | 48.7% |

### 10.2 距离加权最小二乘密度梯度（2026-09-12 已接入并测试）

保留密度传感器、当前物理面积定义的 `h = sqrt(A)`、四兄弟减疏及 2:1 平衡，只更换 AMR 使用的密度梯度估计，不改变流体方程的重构、通量或重映射。

对叶子单元 K 的面邻居 j，记当前物理中心差为 d_j = x_j - x_K，密度差为 Δρ_j = ρ_j - ρ_K。计算

\[
\boldsymbol g_K=\underset{\boldsymbol g}{\arg\min}
\sum_j\frac{(\boldsymbol d_j\cdot\boldsymbol g-\Delta\rho_j)^2}{|\boldsymbol d_j|^2},
\qquad |\nabla\rho|_K=|\boldsymbol g_K|.
\]

- 用距离归一化的两列矩阵和流式 Givens QR 求解，不显式求正规矩阵的逆。
- 粗细接口的两个细邻居分别作为观测；物理边界只用域内邻居进行单边拟合，不在域外补零密度。
- 进入新梯度路径时刷新 ghost 密度和中心坐标；只写本进程叶子单元，不写 ghost。
- 梯度模写入已有 `idCDensityGradient`；拟合矩阵只用临时叶子数组，不扩充叶子持久数据结构。边差商字段保留为诊断数据，角点密度梯度清零，不参与新估计。
- 非有限输入、重合中心、邻居缺失或方向退化（拟合矩阵 σ_min/σ_max ≤ 1e-10）返回无穷大指标，阻止减疏并允许层级范围内加密；运行时每进程首次出现退化会报警。有限精度阈值是保护规则，不是误差估计器。

配置：

```ini
density_gradient_method = 1   # 默认：距离加权最小二乘；0：旧最大边差商
refine_coarsen_enum = 6        # 无量纲密度指标；编号0也可使用新梯度
refine_err = 0.20
coarsen_error = 0.08
```

该选择器不影响压力或距离判据。新梯度恢复原先被低估的方向后，旧阈值可能标记更多单元，不保证减少网格，也不保证只识别激波。非线性场、间断及严重畸变仍需数值验证；本轮不同时调整 h 或阈值。

验证入口：`tools/test_density_least_squares.cpp`（数值内核、真实 p4est 单树/多树、边界、粗细接口、ghost 刷新、空进程），以及 `tools/test_density_gradient_snapshot.cpp`（保存的 Sedov 畸变网格制造场）。1/2/4 进程的线性场测试均通过，多树最大梯度分量误差约 5.6e-14。

同一旧 Sedov 畸变网格上，ρ=r² 的对角区梯度模/解析值中位数从 0.1970 改善至 0.9611；ρ=r⁵ 从 0.1991 改善至 0.9545。它证明这些制造场的方向低估明显改善，不等于所有流动的误差都同比降低。

完整串行 Sedov 对照：L4–L6、0.20/0.08、每4步 AMR，两种方法均运行4617步至 t≈1.000568，开启初始化及每接受步的状态一致性检查。旧方法末态1735叶子，新方法2446叶子；末态总能量相对误差分别约6.92e-15和1.07e-14。4进程新方法另完成至 t≈0.01 的短程集成测试；不能把短程测试表述为全程并行验证或强扩展测试。

本轮结果及审计放在 `.tmp/least-squares-gradient-20260912/`，交付串行结果在 `sedov-verified/`，旧方法对照在 `legacy/`。末个 VTU 是 t=0.9995583385 的步前快照，串行 `DistanceProfiles.plt` 是推进后的终态剖面，不能按相同时间直接混用。根目录配置和正式 bin 未覆盖；尚未提交 Git。

### 10.3 密度—压力联合识别与压缩辅助 AMR（2026-09-16 隔离试验方案）

**数学物理思想：密度变化发现待分辨结构，压力跃变确认激波特征，压缩检测补充尚未充分形成的跃变信号。** 目标是保留主要激波结构，同时减少近压力平衡的波后密度扰动引起的误加密；这是提高加密选择性，不是平滑物理场或消除全部噪声。判据不依赖解析激波半径或预设传播轨迹。

本节记录 Noh、Sedov 已完成的隔离候选试验，**尚未合入正式求解器，不替代第 7 节基础算例配置**。两者共用数学框架，但压力归一化与阈值不同。

#### 指标与加密减疏条件

密度指标沿用 10.1–10.2 节：\(\eta_{\rho,K}=\sqrt{A_K}|\boldsymbol g_K|/\max(|\rho_K|,\rho_{\mathrm{floor}})\)，梯度由面邻居加权最小二乘得到，尺度取当前物理面积的平方根。它衡量单元尺度上的相对密度变化，不能单独区分激波、接触间断和扰动。

增加压力指标与独立压缩指标：

\[
P_K^{(\alpha)}=\max_{j\in\mathcal N(K)}
\frac{|p_j-p_K|}{|p_K|+|p_j|+\alpha p_*+10^{-12}},
\qquad p_*=\max_{\text{全域叶子}}|p|,
\]

\[
C_K=\frac{\max(-\Phi_K,0)}{\sqrt{A_K^{\mathrm{geom}}}\max(U_*,10^{-12})},
\qquad
\Phi_K=\sum_{e\subset\partial K}\overline{\boldsymbol u}_e\cdot\boldsymbol n_e\,|e|.
\]

其中全局最大值通过 MPI 归约取得；\(A_K^{\mathrm{geom}}\) 由当前角点计算，\(\boldsymbol n_e\) 为外法向，边速度取两端节点速度平均，\(U_*\) 为全域单元中心及节点速度模的最大值。\(C_K\) 相当于归一化的负散度，仅响应压缩。

压力全局尺度（\(\alpha=1\)）抑制低压力区绝对幅度很小的相对扰动，但可能漏掉弱波；局部尺度（\(\alpha=0\)）更重视当地相对变化。对有效状态及允许层级：

\[
\text{加密：}\quad
\ell_K<L_{\max}\ \land\
\left[(\eta_{\rho,K}>\eta_r\ \land\ P_K>P_r)\ \lor\ C_K>1\right],
\]

\[
\text{家族减疏：}\quad
\forall K\in F:\quad C_K<0.5\ \land\
(\eta_{\rho,K}<\eta_c\ \lor\ P_K\le P_c).
\]

即：**密度与压力联合触发主通道，强压缩可独立请求加密；主信号之一减弱且压缩不强时，才可能退出细网格。** F 必须是完整四兄弟家族，且全部通过最低层级、减疏安全标签及拓扑约束；非有限状态不得按光滑单元处理。

| 候选算例 | 压力尺度 \(\alpha\) | 密度阈值 \(\eta_r/\eta_c\) | 压力阈值 \(P_r/P_c\) | 层级 / AMR 周期 |
|---|---:|---|---|---|
| NohCartesian | 1：全局尺度 | 0.20 / 0.19 | 0.20 / 0.10 | L4–L7 / 每4步 |
| SedovCartesian | 0：局部尺度 | 0.20 / 0.19 | 0.05 / 0.025 | L4–L7 / 每4步 |

#### 计算流程

1. **检测：**到达 AMR 检查步，刷新几何、邻居及 ghost 数据，计算密度梯度、压力和压缩指标。
2. **加密：**逐叶子判断、细分并转移流体状态，记录本轮新产生的家族。
3. **减疏：**刷新所需邻接信息，检查四兄弟条件及安全标签；禁止新家族同轮立即合并，并预演排除合并后会被 2:1 balance 立即拆回的家族。
4. **一致性恢复：**执行获准减疏、corner 2:1 balance 及按配置需要的重分区；刷新状态，按状态方程同步压力、声速等派生量。
5. **时间推进：**继续拉格朗日水动力更新，保留状态、几何和守恒检查，到下一检查步重复。

同轮保护不能仅由阈值滞回替代：子单元继承父密度和梯度而尺度约减半时，\(\eta_{\rho,\mathrm{child}}\approx\eta_{\rho,\mathrm{parent}}/2\)，可能刚加密便满足减疏条件。拓扑预演则避免无效的“合并—平衡拆分”数据往返；部分波后细网格属于必要过渡层，不一定由当地传感器超阈值造成。

**配套处理与边界：**两组候选还使用悬挂点修正的内能权重 \(w_i=m_i e_i/\sum_jm_je_j\)，按 \(\boldsymbol R_i=w_i\boldsymbol R\) 分配修正。这保持合计修正向量，却改变局部更新，属于粗细网格耦合而非加密判据，不能据此宣称保正性或熵稳定性。当前效果来自上述措施的组合；压力 AND 偏重激波，可能削弱近等压接触间断的分辨率，Sod 对照已提示这一限制，不能仅凭网格减少认定精度改善。

可复算归档见 [密度—压力 AMR 试验包](experiments/density-pressure-amr/README.md)，包含隔离源码、Noh/Sedov 成功配置、编译运行入口及结果校验依据；不覆盖正式 src/。详细历史诊断仍保存在本地 .tmp/noh-sensor-study-20260914/ 中，复算归档包不依赖该临时目录。

### 10.4 其余数学方案待补充

待补充内容：

- [ ] 拉格朗日形式的控制方程；
- [ ] 单元中心物理量与节点运动学量；
- [ ] 动量方程和总能量/内能更新；
- [ ] 节点速度、角点力与阻抗构造；
- [ ] 非一致网格及悬挂节点约束；
- [ ] AMR 加密、减疏和重映射的数学定义；
- [ ] 继续用误差范数和激波/接触间断位置验证已标定阈值，并比较不同 `h_K` 定义；
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
