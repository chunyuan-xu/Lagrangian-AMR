# Lagrangian-AMR

`Lagrangian-AMR` 是一个基于 **p4est** 的二维拉格朗日可压缩流求解器，支持非一致四边形网格、MPI 并行、叶子单元上的自适应加密与减疏（AMR）、VTU/PVTU 输出、守恒误差记录和距离剖面输出。

本文档是当前 AMR 方法的技术基线，也可作为论文“自适应网格方法”初稿的直接输入。为避免历史试验路线干扰写作，本文保留两条通用传感器方案，以及一条用于对照与诊断的环带加密策略：

1. **无量纲密度梯度 AMR**：根目录求解器中可直接配置的基础方案；
2. **common-weights AMR（CW-AMR）**：密度—压力共同服务变量、共享几何权重的扩展方案，当前位于隔离试验包中。
3. **环带加密**：以诊断到或预先给定的波前半径为中心的几何对照策略。

除上述三类策略外，其余历史 AMR 路线及参数均已从本文移除。它们如有必要可从 Git 历史恢复，但不应作为当前论文方法的依据。

> 文档范围：截至 2026 年 9 月。本文中的“已完成基础计算”只表示某一配置已正常完成数值试验；除非另有独立收敛性、守恒性和跨算例验证，它不构成普适性或严格精度证明。

## 1. 目录与输出

```text
Lagrangian-AMR/
├── src/                                  # 根目录求解器：无量纲密度梯度 AMR
│   ├── amr/
│   │   ├── least_squares_gradient.h      # 最小二乘梯度重构
│   │   ├── density_gradient.h            # 密度梯度指标
│   │   └── amr_criteria.h                # 加密/减疏与家族安全逻辑
│   ├── hydro/hydro_controller.h          # 时间推进中的 AMR 调度
│   └── main.cpp
├── experiments/density-pressure-amr/     # 隔离的 CW-AMR 试验包
│   ├── src/amr/linear_service_sensor.h   # 共同权重服务变量传感器
│   ├── src/amr/amr_callbacks.h           # CW-AMR 回调与标记接口
│   └── tests/run_unified_cw.py           # 统一 CW 基础算例驱动
├── include/                              # 头文件或外部接口
├── third_party/p4est/                    # p4est 与本地安装目录
├── param.ini                             # 根目录默认运行参数
├── Makefile                              # 当前可用构建入口
└── README.md                             # 本文档
```

常用运行输出：

```text
output/p4est_Lagrangian_*.pvtu            # ParaView 并行索引
output/p4est_Lagrangian_*_*.vtu           # 各 MPI 进程网格与物理场
output/p4est_Lagrangian_*.visit           # VisIt 索引
EnergyError.plt                           # 能量误差历史
DistanceProfiles.plt                      # 距离/半径剖面
```

## 2. 构建与运行

### 2.1 依赖

- 支持 C++14 的编译器；
- MPI C++ 环境；
- p4est、libsc、zlib；
- Windows 链接时的 `ws2_32`；
- CMake 3.18+ 与 Ninja（供后续恢复 CMake 工作流使用）。

当前 Windows 开发环境采用 MSYS2 UCRT64 GCC、Microsoft MPI 与仓库内 `third_party/p4est` 的本地安装。

### 2.2 当前可用的 Makefile 工作流

当前根目录没有顶层 `CMakeLists.txt`，因此 `cmake -S . -B build-cmake` 不是可用入口。请使用 Makefile：

```powershell
# 按本机安装位置调整 PATH
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
make clean
make -j4
mpiexec -n 4 .\AMR_Solver.exe .\param.ini
```

在 MSYS2 shell 中通常为：

```bash
make clean
make -j4
mpiexec -n 4 ./AMR_Solver.exe ./param.ini
```

每次变更 AMR 参数后，建议使用独立输出目录或保存对应的 `param.ini` 副本，避免不同配置的剖面和能量历史相互覆盖。

### 2.3 运行后检查

确认程序正常结束，并检查 `EnergyError.plt`、`DistanceProfiles.plt`、最终与峰值单元数、最小密度/压力/内能/面积、网格质量以及 MPI 负载不均衡。含减疏的算例还应检查粗细网格交界与减疏前后的物理场。

## 3. 两种方案共用的 AMR 执行框架

两种传感器采用同一 AMR 生命周期：

```text
叶子单元状态与几何
  → 更新邻域和单元尺度
  → 计算局部 AMR 指标
  → 用阈值滞回标记加密或候选减疏
  → 对完整同父四兄弟家族执行减疏安全检查
  → p4est refine/coarsen/balance/repartition
  → 状态重映射并进入下一时间步
```

### 3.1 阈值滞回与家族减疏

对任意单元指标 `I_K`，加密阈值 `tau_r` 和减疏阈值 `tau_c` 应满足

\[
\tau_c < \tau_r.
\]

当 `I_K > tau_r` 时标记加密。减疏不是单个叶子单元的删除：只有完整同父四兄弟家族 `F` 的全部子单元均满足 `I_K < tau_c` 时，`F` 才是减疏候选。阈值间隔提供滞回，以减少同一位置反复加密—减疏带来的拓扑抖动与重映射误差累积。

候选家族还必须通过最低层级、2:1 平衡、状态正性、几何质量、拓扑预演和同轮保护等约束。因此“指标低于阈值”只是减疏的必要条件，不保证会立即发生减疏。

### 3.2 共同的物理边界

AMR 重新分配自由度，并不能恢复已经被粗网格演化或重映射抹平的波前信息。对强激波，初始分辨率、传感器响应时机、AMR 调度周期和重映射共同决定最终误差；不能仅依赖后期加密弥补早期欠分辨造成的信息损失。

## 4. 方案 A：无量纲密度梯度 AMR

### 4.1 数学定义

对叶子单元 `K`，以单元中心最小二乘重构密度梯度 `∇rho_K`，取二维单元特征长度

\[
h_K=\sqrt{A_K},
\]

其中 `A_K` 是当前拉格朗日物理四边形面积。无量纲密度梯度指标定义为

\[
\eta_{\rho,K}=h_K\frac{|\nabla\rho_K|}
{\max(|\rho_K|,\varepsilon_\rho)}.
\]

`epsilon_rho` 仅用于避免低密度区域的归一化奇异。`h_K|∇rho|/|rho|` 可理解为一个单元尺度上的相对密度变化，因此不会因整体密度量级改变而失去可比性。`h_K` 随拉格朗日网格运动与变形更新，不应简单用 p4est 逻辑层级尺寸取代。

### 4.2 加密/减疏逻辑

\[
\eta_{\rho,K}>\tau_r\Rightarrow\text{refine},
\]

\[
\forall K\in F:\quad\eta_{\rho,K}<\tau_c
\Rightarrow\text{coarsen family }F,\qquad\tau_c<\tau_r.
\]

其中 `F` 是一个完整同父四兄弟叶子单元家族，最终仍受第 3 节安全约束。

### 4.3 数学物理思想与局限

密度梯度同时对接触间断、压缩波和激波敏感，形式简单、计算成本低、参数含义直观，适合作为基础 AMR 路线和传感器对照基线。

局限在于强激波波后的微小数值噪声也可能形成较大的相对密度梯度，进而阻止减疏或触发非目标加密。因此它不是严格的激波判别器；应结合阈值滞回、合理的减疏周期和最终网格分布检查来评价。

### 4.4 实现位置与配置

```text
src/amr/least_squares_gradient.h
src/amr/density_gradient.h
src/amr/amr_criteria.h
src/hydro/hydro_controller.h
```

典型配置字段：

```ini
refine_coarsen_enum = 6
density_gradient_method = 1
minus_level = 4
max_level = 6
refine_err = <tau_r>
coarsen_error = <tau_c>
refine_period = <positive integer>
```

阈值、层级和 AMR 周期必须按算例标定；“无量纲”不表示参数天然与分辨率、流动状态和时间调度无关。

## 5. 方案 B：common-weights AMR（CW-AMR）

### 5.1 设计目标

CW-AMR 面向强间断流动中“单一密度传感器在波后噪声区过度响应”的问题。它不改变守恒变量、状态方程或水动力通量；而是构造仅服务于 AMR 决策的密度—压力特征变量，并以相同的物理邻域和几何权重评价二者的局部变化。

密度主要表征激波和接触结构，压力补充压缩波信息。二者共享邻域和权重，使比较建立在同一局部几何模板上，而不会因几何采样不同产生不一致标记。

### 5.2 对数服务变量

对每个叶子单元 `K`，定义

\[
q_\rho=\ln\frac{\rho+\rho_*}{\rho_{\rm ref}},\qquad
q_p=\ln\frac{p+P_*}{p_{\rm ref}},
\]

其中

\[
\rho_*=f_\rho\rho_{\max},\qquad
P_*=\frac{1}{2}\alpha p_{\max}.
\]

`rho_*` 与 `P_*` 是 AMR 服务变量的正则/尺度参数，用来抑制极小状态量下对数相对变化的过度放大。它们**不是**守恒变量、EOS 或流体通量使用的 density/pressure floor，不应被描述为对物理解或守恒更新施加截断。

### 5.3 共享权重的线性重构

对密度和压力服务变量分别进行相同权重的局部线性最小二乘拟合。对邻居 `j ∈ N(K)`，令

\[
w_{Kj}=|\boldsymbol{x}_j-\boldsymbol{x}_K|^{-2},
\]

\[
\boldsymbol g_{s,K}=\arg\min_{\boldsymbol g}
\sum_{j\in N(K)}w_{Kj}[q_{s,j}-q_{s,K}-\boldsymbol g\cdot
(\boldsymbol{x}_j-\boldsymbol{x}_K)]^2,\qquad s\in\{\rho,p\}.
\]

以 `h_K=sqrt(A_K)` 构造基础联合特征：

\[
S_K=h_K\sqrt{|\boldsymbol g_{\rho,K}|^2+|\boldsymbol g_{p,K}|^2}.
\]

采用平方和而非带符号相加，可避免密度和压力梯度方向不同而彼此抵消。

### 5.4 残差—活动度门控（mode 8）

当前强间断算例可通过残差和活动度门控，降低平滑背景与小振幅噪声的误触发。令

\[
\chi_s=\sqrt{\frac{\sum_j[\Delta q_{s,j}/d_j-
\boldsymbol g_{s,K}\cdot\widehat{\boldsymbol d}_{Kj}]^2}
{\sum_j(\Delta q_{s,j}/d_j)^2}},\qquad
G_s=\frac{\chi_s}{\chi_s+\chi_0},
\]

\[
a_s=\sqrt{\frac{\sum_j(\Delta q_{s,j})^2}
{\sum_j(\Delta q_{s,j})^2+N_K\chi_0^2}},
\]

其中 `d_j=|x_j-x_K|`，`hat(d)_Kj` 是对应方向单位向量，`N_K` 是邻居数，`chi_0` 是小量正则尺度。最终指标为

\[
S_K^{(8)}=h_K\sqrt{(a_\rho G_\rho|\boldsymbol g_{\rho,K}|)^2+
(a_pG_p|\boldsymbol g_{p,K}|)^2}.
\]

残差门控降低近似线性平滑场的响应；活动度门控降低极小振幅扰动的影响。若启用独立压缩救援通道 `C_K`，其职责是避免弱但真实的压缩波被门控过度压制，而非替代联合传感器。

### 5.5 加密/减疏判定与物理思想

\[
S_K^{(8)}>\tau_r\ \lor\ C_K>C_r\Rightarrow\text{refine},
\]

\[
\forall K\in F:\quad S_K^{(8)}<\tau_c\land C_K<C_c
\Rightarrow\text{coarsen family }F,\qquad\tau_c<\tau_r.
\]

若未启用压缩救援通道，可忽略 `C` 条件。该路线的核心思想是：对数服务变量提取跨数量级流动中的相对变化；共同几何模板确保密度和压力可一致比较；联合平方范数同时保留激波、接触和压缩响应；残差/活动度门控减少平滑背景和微小噪声的驱动；阈值滞回与家族安全条件限制无意义的拓扑往复。它追求将自由度集中于持续存在的非平滑或压缩结构，而非承诺完全无噪声。

### 5.6 实现位置、接口与基础配置

CW-AMR 当前是隔离试验包，而非根目录默认求解器的一部分：

```text
experiments/density-pressure-amr/src/amr/linear_service_sensor.h
experiments/density-pressure-amr/src/amr/amr_callbacks.h
experiments/density-pressure-amr/tests/run_unified_cw.py
```

常用环境变量：

```text
AMR_WENO_MODE=8
AMR_WENO_REFINE=<tau_r>
AMR_WENO_COARSEN=<tau_c>
AMR_CW_DENSITY_FLOOR=<f_rho>
AMR_PRESSURE_FLOOR_FACTOR=<alpha>
AMR_CW_RESIDUAL_FLOOR=<chi_0>
AMR_CW_ACTIVITY_FLOOR=<activity_scale>
AMR_COMPRESSION_REFINE=<C_r>
```

变量名中的 `WENO` 为历史兼容命名；当前关键操作是共同权重的线性服务变量重构，并不意味着已经实现高阶 WENO 重构。

下表列出已经完成完整计算的基础配置。统一的是数学框架；各问题的服务尺度和阈值不同，不能写成“一组参数适用于所有算例”。

| 算例 | 路线 | 层级 | 主要传感器参数 |
| --- | --- | --- | --- |
| Sedov | residual / mode 8 | L5–L8 | `f_rho=0.05`, `alpha=0.05`, `chi_0=0.05`, `tau_r/tau_c=0.20/0.19` |
| Noh | residual / mode 8 | L5–L8 | `f_rho=0.05`, `alpha=1`, `chi_0=0.05`, `tau_r/tau_c=0.20/0.19` |
| SodCartesian | scaled / mode 7 | L5–L8 | `f_rho=0.05`, `alpha=1`, `tau_r/tau_c=0.08/0.06` |
| TwoDimRiemann | residual / mode 8 | L5–L8 | `f_rho=0`, `alpha=1`, residual `0.01`, activity `0.10`, `tau_r/tau_c=0.08/0.06` |

Sedov 的一组保留候选使用初始全域 L8、运行层级 L5–L8、`tau_r/tau_c=0.20/0.19`、`t<0.001` 禁止减疏、加密周期 4 步和减疏周期 500 步。其用途是考察激波初期分辨率和减疏时机对波前的影响；它不是可脱离网格、时间步长和重映射方法而普适复用的参数组。

## 6. 方案 C：环带加密（波前对照策略）

### 6.1 定义与用途

环带加密将单元中心到指定中心的距离与波前半径比较。记

\[
r_K=|\boldsymbol{x}_K-\boldsymbol{x}_c|,
\]

其中 `x_c` 为爆源或对称中心，`R_f(t)` 为诊断得到、参考解给出或预先指定的波前半径。给定半带宽 `delta_R`，环带为

\[
|r_K-R_f(t)|\le\delta_R.
\]

环带内单元被加密或保持高层级；环带外单元可按层级规则减疏。该策略直接利用波前位置信息，因此非常适合：

1. 将传感器 AMR 与“已知波前附近保留自由度”的理想化对照相比较；
2. 诊断误差来自传感器漏捕捉，还是来自初始分辨率、重映射或求解器本身；
3. 在轴对称 Sedov/Noh 类问题中构造可控、可重复的网格分布。

### 6.2 适用边界

环带加密不是不依赖先验信息的通用 AMR 判据：它要求可信的 `R_f(t)` 或可靠的数值波前诊断。若将解析半径直接注入计算，它只能作为控制/验证实验，不能与纯局部传感器方案等价比较。论文中应明确波前半径的来源、带宽、更新周期以及是否使用解析信息。

对于非径向、波前分叉或多间断相互作用问题，单一圆形环带通常不适用；此时应使用无量纲密度梯度或 CW-AMR。环带宽度过窄会漏掉移动波前，过宽则削弱效率优势，因此应报告 `delta_R` 相对于局部网格尺度和波前移动距离的关系。

### 6.3 实现与调用位置

根目录历史实现以 `src/amr/amr_criteria.h` 中的距离/半径判断逻辑为基础，并由 `src/hydro/hydro_controller.h` 的 AMR 调度调用。若当前分支回退后需要恢复，应优先恢复为独立的“距离环带判据”函数，并保留如下明确输入：中心 `x_c`、当前波前半径 `R_f(t)`、环带宽度 `delta_R`、最低/最高层级、加密/减疏周期。

环带策略的数学和实现应与密度梯度/CW 路线分开维护：前两者从局部流场量自适应识别结构；环带则从几何距离和外部或诊断的波前位置分配网格。

## 7. 论文写作时的方案选择与边界

| 目标 | 推荐路线 | 论文中必须说明 |
| --- | --- | --- |
| 低成本基线、直接可配置的 AMR | 无量纲密度梯度 | 阈值、周期、最终层级分布，以及波后噪声敏感性 |
| 减少远离波前的非目标加密 | CW-AMR | 当前为隔离试验实现；以对照计算说明额外复杂度的收益 |
| 分离“波前识别”与“数值格式/重映射”误差 | 环带加密 | 波前半径的来源、带宽和更新周期；它不是无先验通用判据 |

对任一方案，建议报告初始网格、最小/最大层级、加密与减疏阈值、调度周期、最终/峰值单元数、负载不均衡、守恒误差、波前位置或剖面误差，并展示减疏前后的网格和物理场。单一终态云图不足以证明 AMR 的可靠性。

## 8. 让智能体据此起草论文方法

可以直接在新对话中 @ 本 README，让智能体撰写论文“自适应网格方法”初稿。建议提示词：

```text
请阅读 C:\ai\Lagrangian-AMR\README.md。
仅依据其中“AMR 共用执行框架”“无量纲密度梯度 AMR”、
“common-weights AMR”和“环带加密”部分，撰写中文论文的
“自适应网格方法”初稿。

将三种策略分别成节：前两种是基于局部流场的传感器方法；
环带加密是依赖波前位置的几何对照策略。保留数学定义、阈值滞回、
完整家族减疏条件、核心数学物理思想和实现范围；明确 CW-AMR
当前位于隔离试验包。

不要把“已完成基础计算”写成普适性、严格收敛性或解析精度证明；
不要加入 README 未给出的算法细节、实验数值或参考文献。
```

若投稿英文期刊，可将“撰写中文论文”替换为 “draft an English Methods section”，并要求符号体系与论文其余章节一致。论文结果部分仍应另行提供具体配置、结果表和图；README 记录的是方法技术基线，而非完整实验数据档案。

## 9. 后续工作占位

### 程序方案

待补充：叶子单元数据布局、p4est 拓扑迁移、状态重映射、并行负载平衡和 I/O 设计。

### 数学方案

待补充：空间离散、Riemann 解、时间推进、人工黏性/耗散，以及 AMR 状态转移的一致性分析。

### Verification & Validation（V&V）

待补充：Sedov、Noh、SodCartesian 与二维 Riemann 问题的基准定义、网格收敛、守恒性、负载平衡、运行效率和后处理流程。
