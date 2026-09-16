# Common-weights AMR：线性共享平滑指标（2026-09-16）

项目数学方案见根目录 `README.md` 第 10.4 节。CW-AMR 表示借鉴 common-weights 思想的共享线性平滑判据：两分量使用共同的几何拟合权重，并合成一个网格标记指标；不是 WENO 非线性权重或通量重构的实现。

这是独立实验，不替换根目录 `src/`，不更新黄金参考值。运行结果位于
`.tmp/weno-linear-study-20260916/`；结论以该目录的 `REPORT.md` 为准。

## 数学来源与适配边界

参考 Shen 等人的 SSRN 4257437 预印本《A robust common-weights WENO scheme based on the flux vector splitting for Euler equations》，第 3.3 节、式 (22)。原文用

\[
\Gamma^\pm=\rho p f_E^\pm
\]

计算同一组 WENO 权重，使各守恒分量采用一致的模板贡献。密度可感知接触间断和激波，压力强化激波信息，分裂能量通量还携带速度和迎风信息。这是**通量重构方法，不是现成的 AMR 判据**。

与原 AMR 方法的共性是“单元尺度上的变化强弱”。原密度指标

\[
\eta_\rho=\sqrt{A_i}|\nabla\rho|/\rho_i
\simeq h_i|\nabla\log\rho|,\qquad h_i=\sqrt{A_i}
\]

与线性多项式的平滑指标 `beta = h^2 |gradient|^2` 相对应。此处只借用平滑度共享思想：**不改变水动力通量，不实现高阶 WENO，不把标记器当作消除数值噪声的算法**。

## 两种线性路线及压力尺度保护

1. 标量服务变量（mode 4）：恒定 gamma、静止状态下，Steger–Warming 分裂满足 `|fE| = rho*c^3/[2*gamma*(gamma-1)]`，故 `|rho*p*fE|` 正比于 `rho^(1/2)*p^(5/2)`。取 `q=(log rho+5 log p)/6`，指标为 `h|grad q|`。这是忽略速度因素的 AMR 适配，不等价于论文完整算法；相反方向的梯度可能抵消，密度接触的权重也偏弱。
2. 共享线性平滑指标（mode 5）：分别线性拟合 `log rho`、`log p`，合并 `S=sqrt(beta_rho+beta_p)`。避免变量之间的梯度抵消，恒压接触不会被压力 AND 条件抹掉。但极小压力的相对扰动仍可能被放大。
3. 带压力尺度保护的共享指标（mode 6）：沿用原程序的压力尺度参数，令

\[
P_*=\tfrac12 f_P\max_j p_j,\quad
q_\rho=\log(\rho/\rho_{ref}),\quad
q_p=\log[(p+P_*)/p_{ref}],\quad
S_i=h_i\sqrt{|\nabla q_\rho|^2+|\nabla q_p|^2}.
\]

每次标记时 `P_*` 是空间常数；局部压力变化远小于该尺度时，`grad q_p = grad p/(p+P_*)` 不会由于接近零的分母而放大。系数 1/2 使分母与原对称压力跳变的 `pa+pb+fP*pmax` 有对应关系。`fP` 直接复用 `AMR_PRESSURE_FLOOR_FACTOR`（Noh=1；Sedov、径向 Sod=0）。**这不是给物理压力加底限，不修改状态或 EOS。**

正参考量只用于定义无量纲对数。代码只计算 `log(b)-log(a)`，参考常数自然消去；压力单位变化时 `P_*` 同比例变化，指标保持不变。仅在正密度、正压力和有效几何下计算，非法状态不会被解释为光滑。

## 线性拟合与标记流程

对当前物理面邻居（包括粗细面子邻居和 MPI ghost），采用现有加权最小二乘：

\[
g_i=\arg\min_g\sum_j[(q_j-q_i-g\cdot(x_j-x_i))/|x_j-x_i|]^2.
\]

复用现有 QR 求解和物理面积，不需要增加模板环数。服务变量仅服务于标记，不用于 AMR 重映或通量求解。

1. 刷新状态、现有压缩指标和全局压力尺度。
2. 在原面遍历中累计两个对数场的线性拟合。
3. 保存共享指标 `idAMRWenoSensor`。
4. `S > refine` 请求加密；四个兄弟都满足 `S < coarsen` 才有减疏资格。
5. 保留现有压缩补救、禁止减疏标志、减疏计划和 2:1 平衡；不绕过覆盖、面积或状态检查。

`0.20 / 0.19` 是本轮候选阈值，不声称通用最优。`0.20 / 0.12` 用于早期宽滞回对照。`AMR_WENO_MODE` 是实验沿用的开关名称；mode 4–6 没有 WENO 非线性权重，也没有高阶重构。mode 1–3 的早期两点通量跳变原型仅保存在对应旧 run 的源码快照中，当前源码已移除并拒绝使用：其法向投影并非完整二维分裂通量，且旧能量项系数也不宜用来忠实验证论文。它们不是本次推荐方法的证据。新服务变量关闭时保持原方法；打开后默认 mode 6。

## 编译与复现

```powershell
./experiments/density-pressure-amr/build.ps1 -Jobs 3

# 新 prefix 必须唯一，脚本拒绝覆盖已有实验。
py -3 experiments/density-pressure-amr/tests/run_service_study.py --prefix my-screen --cases noh sedov sod --modes 0 6 --end .05 --coarsen .19 --limit 180
py -3 experiments/density-pressure-amr/tests/run_service_study.py --prefix my-full --cases noh --modes 6 --end .6 --coarsen .19 --limit 0
py -3 experiments/density-pressure-amr/tests/run_service_study.py --prefix my-full --cases sedov --modes 6 --end 1 --coarsen .19 --limit 0
py -3 experiments/density-pressure-amr/tests/run_service_study.py --prefix my-full --cases sod --modes 6 --end .2 --coarsen .19 --limit 0
```

默认 L5–L8、MPI 1、每 4 步 AMR，求解器与重映开关来自包内原成功 presets。mode 0 是关闭新服务变量的原密度—压力方法。`sod` 指半径 0.5 的二维径向 SodCartesian、gamma=5/3，不是标准 gamma=1.4 的一维激波管；`sod1d` 是额外可选的 Sod1DCartesian。

每次运行保存配置、完整环境、二进制/源码快照与 SHA256、日志、VTK、`DistanceProfiles.plt` 和结果 JSON。早期超时是成本筛选，不是数值失败。并发试验的运行时间不用于效率结论。

`linear_service_test.cpp` 是不依赖 p4est 的数学单测，覆盖常值、对数仿射精度、h 缩放、单位变换、接触、反向梯度抵消、无效状态、冷区压力保护。可从仓库根目录编译运行（示例输出到已有的 `.tmp` 目录；不要定义 `NDEBUG`）：

```powershell
New-Item -ItemType Directory -Force .tmp | Out-Null
g++ -std=c++14 -O2 -Iexperiments/density-pressure-amr/src experiments/density-pressure-amr/tests/linear_service_test.cpp -o .tmp/linear-service-test.exe
./.tmp/linear-service-test.exe
```

运行脚本面向本项目现有 Windows/MSYS2/MS-MPI 环境，默认对比 mode 0/6、阈值 0.20/0.19；不是跨平台安装器。本地研究分析脚本 `analyze_service_study.py` 依赖未归档的 `.tmp/noh-sensor-study-20260914/` 与 `tools/rebuild_shock_history.py`，本次不纳入 Git，也不是构建、单测或完整计算的依赖。原始大体积运行输出保留本地，Git 保存数学方案、结果摘要、实现和复算入口。

## 尚需谨慎的解释

- 标记器改变分辨率分布，不直接增加耗散，不能保证消除波后噪声。真实密度接触与密度噪声都可能触发密度分量。
- 全局压力尺度会降低对远离强激波的弱压力波的敏感性，这是继承原设置的代价。
- 相比原压力 AND，这条路线可能改善接触识别，也可能保留更多波后网格；必须看完整时程和波结构，不能只看单元数。
- `h=sqrt(A)` 是等效二维长度；它不是强畸变网格的各向异性误差度量。
- Saved VTK 是有限精度输出。基于其坐标重算的质量漂移仅为辅助诊断，不能代替求解器内双精度守恒检查。
- 测试失败时记录实际触发的检查，不把覆盖/面积错误自动归因于“减疏过宽”或“传感器公式错误”。
