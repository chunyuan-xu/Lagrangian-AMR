# 面向非协调 Lagrangian-AMR 的解耦主点-悬点节点求解器

本文给出一种面向 2:1 非协调 Lagrangian-AMR 网格的节点求解器设计。悬点不是独立自由度，其位置和速度由相邻两个主点插值得到；同时，为保持现有节点求解器的局部性，每个主点仍独立求解一个 $2\times2$ 线性系统。

本文将主要方法命名为：

> **解耦支路广义力格式**（decoupled branch generalized-force formulation，简称 DBGF）。

DBGF 在每个悬点角构造两个只依赖各自主点速度的声学支路，以支路广义力及其共轭主点速度计算功。该格式保持主点系统完全解耦，并在半离散层面自然满足动量守恒、总能量守恒和几何守恒律（GCL）。它相对于严格约束虚功格式增加一个与主点相对速度有关的非负耗散。

作为参照，本文还简要给出：

1. **物理悬点功闭合表示**：在能量方程中使用物理悬点力与物理悬点速度做功，并显式加入功率闭合项；其局部版本与 DBGF 是同一半离散更新的两种记账方式。
2. **约束虚功耦合格式**：将唯一的物理悬点力按运动学约束的转置严格映射到两个主点。该方法具有最直接的约束力学解释，但会在主点之间产生非对角耦合。

本文是数学设计文档，不表示当前 C++ 代码已经实现该方案。

---

## 1. 设计目标、范围与基本假设

### 1.1 两个独立的设计约束

本文同时采用以下两个约束：

1. **运动学约束**：悬点 $h$ 没有独立自由度，只保留主点 $a,b$ 的自由度；
2. **代数解耦约束**：组装 $a$ 点节点方程时不出现 $\mathbf U_b$，反之亦然。

第一条本身并不推出第二条。严格消去受约束悬点通常会在父自由度之间产生耦合。DBGF 是在额外要求主点独立求解后得到的守恒解耦格式。

### 1.2 本文证明和未证明的性质

本文在以下假设下证明半离散性质：

- 单元质量 $m_c$ 在 Lagrangian 更新阶段保持不变；
- 声阻抗 $Z_c=\rho_ca_c\ge0$，并在当前求解阶段冻结；
- 声学关系关于待求节点速度为仿射函数；
- 两个悬点支路使用相同的单元状态、阻抗和半边几何；
- 插值权重非负、和为一，并在当前几何阶段固定；
- 粗单元的非协调边按细网格节点切分，单元包含完整的虚拟悬点角几何；
- 所有局部通量、节点聚合和单元更新均唯一写入。

本文不宣称已经证明：

- 非线性或速度相关阻抗下的精确支路拆分；
- 任意完全离散时间积分下的机器精度守恒；
- 有限时间步下的密度、内能和压力正性；
- 跨单元重分配功率闭合项后的局部熵稳定性；
- DBGF 在非协调界面处的正式截断误差阶；
- AMR 拓扑切换和守恒重映射过程中的整体等价性。

---

# 第一部分：解耦支路广义力格式

## 2. 拓扑、几何与符号

### 2.1 节点角色

设 hanging node $h$ 位于 master 节点 $a,b$ 之间。记：

- $\mathcal A(c)$：单元 $c$ 的直接 regular/master 角点集合；
- $\mathcal H(c)$：单元 $c$ 的 hanging 角点集合；
- $\mathcal C_0(a)$：在 master $a$ 处具有直接角点的单元集合，不包含悬点凝聚贡献；
- $\mathcal C(h)$：与 hanging node $h$ 邻接的物理单元集合；
- $\mathcal H(a)$：以 master $a$ 为端点的 hanging node 集合；
- $\mathcal E(c,k)$：单元 $c$ 在角点 $k$ 处的半边集合；
- $P_c,\rho_c,a_c,\mathbf U_c$：单元中心压力、密度、声速和速度；
- $Z_c=\rho_ca_c$：当前阶段冻结的声阻抗。

粗单元在非协调边上必须按细网格节点切分成一致的子半边，并为 $h$ 构造虚拟角点几何。否则逐半边压力拆分、几何闭合和守恒证明均不成立。

### 2.2 半边几何

对 $e\in\mathcal E(c,k)$，定义

$$
L_{ck,e}>0,
\qquad
\mathbf n_{ck,e},
\qquad
\mathbf g_{ck,e}=L_{ck,e}\mathbf n_{ck,e},
$$

其中 $\mathbf n_{ck,e}$ 是相对于单元 $c$ 的外法向单位向量。角点几何向量为

$$
\boxed{
\mathbf N_{ck}
=\sum_{e\in\mathcal E(c,k)}\mathbf g_{ck,e}.
}
$$

![解耦主点与悬点压力支路示意图](assets/master-nodal-solver-with-hanging-nodes.svg)

### 2.3 力和更新的符号约定

本文把 $\mathbf F_{ck}$ 定义为单元更新使用的角点压力力，并采用

$$
\boxed{
\frac{d(m_c\mathbf U_c)}{dt}
=-\sum_k\mathbf F_{ck}.
}
$$

总能量方程使用负功号。若 $\mathbf B_a$ 表示外界施加在主点 $a$ 上、与 $\mathbf U_a$ 功共轭的离散广义力，则节点平衡写为“内部角点力 $+\mathbf B_a=0$”。相应的全局动量和总能量边界项分别为

$$
\sum_a\mathbf B_a,
\qquad
\sum_a\mathbf U_a\cdot\mathbf B_a.
$$

---

## 3. 局部声学闭合

### 3.1 逐半边间断关系

沿半边外法向 $\mathbf n_{ck,e}$，采用线性声学间断关系

$$
p_{ck,e}^{*}-P_c
+Z_c(\mathbf U_k-\mathbf U_c)\cdot\mathbf n_{ck,e}=0,
$$

即

$$
\boxed{
p_{ck,e}^{*}
=P_c-Z_c(\mathbf U_k-\mathbf U_c)\cdot\mathbf n_{ck,e}.
}
$$

### 3.2 单元级局部矩阵、右端和角点力

定义

$$
\boxed{
\mathbf M_{ck}
=Z_c\sum_{e\in\mathcal E(c,k)}
L_{ck,e}(\mathbf n_{ck,e}\otimes\mathbf n_{ck,e}),
}
$$

$$
\boxed{
\mathbf b_{ck}
=\mathbf M_{ck}\mathbf U_c+P_c\mathbf N_{ck}.
}
$$

于是

$$
\begin{aligned}
\mathbf F_{ck}(\mathbf U_k)
&=\sum_ep_{ck,e}^{*}\mathbf g_{ck,e}\\
&=P_c\mathbf N_{ck}
-\mathbf M_{ck}(\mathbf U_k-\mathbf U_c)\\
&=\boxed{\mathbf b_{ck}-\mathbf M_{ck}\mathbf U_k}.
\end{aligned}
$$

对任意向量 $\boldsymbol\xi$，

$$
\boldsymbol\xi^T\mathbf M_{ck}\boldsymbol\xi
=Z_c\sum_eL_{ck,e}
(\boldsymbol\xi\cdot\mathbf n_{ck,e})^2\ge0,
$$

所以 $\mathbf M_{ck}$ 对称半正定。直接 master 角点的力记为

$$
\boxed{
\mathbf F_{ca}^{\Pi}
=\mathbf b_{ca}-\mathbf M_{ca}\mathbf U_a.
}
$$

后文所有 $\mathbf M_{ch},\mathbf b_{ch},\mathbf F_{ch}$ 均首先表示单元级局部量；只有带下标 $h$ 而不带 $c$ 时才表示悬点聚合量。

---

## 4. 悬点运动学与 GCL

### 4.1 固定插值约束

采用

$$
\boxed{
\mathbf U_h
=\omega_a\mathbf U_a+\omega_b\mathbf U_b,
\qquad
\omega_a+\omega_b=1,
\qquad
\omega_a,\omega_b\ge0.
}
$$

2:1 中点悬点满足 $\omega_a=\omega_b=1/2$。

瞬时的仿射力拆分只要求两个支路使用相同权重且权重和为一。若同时要求位置约束

$$
\mathbf x_h=\omega_a\mathbf x_a+\omega_b\mathbf x_b
$$

的时间导数仍等于上述 $\mathbf U_h$，则权重在当前几何阶段必须固定。若权重随时间变化，还应包含

$$
\dot\omega_a\mathbf x_a+\dot\omega_b\mathbf x_b,
$$

并重新推导力拆分、功和 GCL。

### 4.2 完整单元几何闭合

单元必须同时包含直接 master 角点和虚拟 hanging 角点：

$$
\boxed{
\sum_{a\in\mathcal A(c)}\mathbf N_{ca}
+\sum_{h\in\mathcal H(c)}\mathbf N_{ch}
=\mathbf0.
}
$$

体积变化采用

$$
\boxed{
\frac{dV_c}{dt}
=\sum_{a\in\mathcal A(c)}\mathbf N_{ca}\cdot\mathbf U_a
+\sum_{h\in\mathcal H(c)}\mathbf N_{ch}\cdot\mathbf U_h.
}
$$

利用几何闭合，可写为

$$
\frac{dV_c}{dt}
=\sum_a\mathbf N_{ca}\cdot(\mathbf U_a-\mathbf U_c)
+\sum_h\mathbf N_{ch}\cdot(\mathbf U_h-\mathbf U_c).
$$

单个悬点角只提供单元散度的一项；只有对完整单元角点求和后，才能得到 $P_c\,dV_c/dt$。

---

## 5. 解耦支路广义力的构造

### 5.1 逐半边支路压力

对同一个单元、同一条悬点半边，构造两个只依赖各自主点速度的支路压力：

$$
\boxed{
\begin{aligned}
\pi_{ch,e|a}
&=P_c-Z_c(\mathbf U_a-\mathbf U_c)\cdot\mathbf n_{ch,e},\\
\pi_{ch,e|b}
&=P_c-Z_c(\mathbf U_b-\mathbf U_c)\cdot\mathbf n_{ch,e}.
\end{aligned}
}
$$

物理悬点压力为

$$
\pi_{ch,e}^{\mathrm{phys}}
=P_c-Z_c(\mathbf U_h-\mathbf U_c)\cdot\mathbf n_{ch,e}.
$$

由于声学关系关于节点速度是仿射函数，严格有

$$
\boxed{
\pi_{ch,e}^{\mathrm{phys}}
=\omega_a\pi_{ch,e|a}
+\omega_b\pi_{ch,e|b}.
}
$$

若阻抗分别按 $\mathbf U_a,\mathbf U_b,\mathbf U_h$ 重算，或闭合本身是非线性的，则该恒等式一般失效。

### 5.2 单元级支路力和物理合力

定义

$$
\boxed{
\begin{aligned}
\mathbf F_{ch,a}
&=\mathbf b_{ch}-\mathbf M_{ch}\mathbf U_a,\\
\mathbf F_{ch,b}
&=\mathbf b_{ch}-\mathbf M_{ch}\mathbf U_b.
\end{aligned}
}
$$

注入两个 master 自由度的解耦广义力为

$$
\boxed{
\mathbf Q_{ch\to a}^{\mathrm D}
=\omega_a\mathbf F_{ch,a},
\qquad
\mathbf Q_{ch\to b}^{\mathrm D}
=\omega_b\mathbf F_{ch,b}.
}
$$

上标 $\mathrm D$ 表示 decoupled。这两个量是离散支路广义力，不应分别称为作用在同一物理位置上的物理力。

两支之和严格恢复物理悬点角点力：

$$
\boxed{
\begin{aligned}
\mathbf F_{ch}^{\mathrm{phys}}
&=\mathbf Q_{ch\to a}^{\mathrm D}
+\mathbf Q_{ch\to b}^{\mathrm D}\\
&=\mathbf b_{ch}-\mathbf M_{ch}\mathbf U_h.
\end{aligned}
}
$$

因此，在给定同一组 $\mathbf U_a,\mathbf U_b$ 和冻结线性声学闭合时，DBGF 不近似单元动量中使用的物理合力；它改变的是该合力在两个 master 自由度之间的广义分配及其功率表示。

### 5.3 局部量和悬点聚合量

对 hanging node $h$ 聚合

$$
\boxed{
\mathbf M_h=\sum_{c\in\mathcal C(h)}\mathbf M_{ch},
\qquad
\mathbf b_h=\sum_{c\in\mathcal C(h)}\mathbf b_{ch}.
}
$$

并定义

$$
\mathbf F_{h,a}=\mathbf b_h-\mathbf M_h\mathbf U_a,
\qquad
\mathbf F_{h,b}=\mathbf b_h-\mathbf M_h\mathbf U_b.
$$

$\mathbf M_h,\mathbf b_h$ 只用于 master 系统组装和聚合残差检查。单元动量、单元功和局部耗散必须使用本单元的 $\mathbf M_{ch},\mathbf b_{ch}$；不得把聚合力写入每个邻接单元。

---

## 6. 完全解耦的 master 节点系统

定义 master $a$ 的直接聚合量

$$
\mathbf M_a^0
=\sum_{c\in\mathcal C_0(a)}\mathbf M_{ca},
\qquad
\mathbf b_a^0
=\sum_{c\in\mathcal C_0(a)}\mathbf b_{ca}.
$$

相应的直接角点聚合力为

$$
\boxed{
\mathbf F_a^{\Pi}
=\mathbf b_a^0-\mathbf M_a^0\mathbf U_a
=\sum_{c\in\mathcal C_0(a)}\mathbf F_{ca}^{\Pi}.
}
$$

DBGF 的节点平衡为

$$
\boxed{
\mathbf F_a^{\Pi}
+\sum_{h\in\mathcal H(a)}
\omega_{a|h}\mathbf F_{h,a}
+\mathbf B_a=\mathbf0.
}
$$

代入矩阵形式得到

$$
\boxed{
\left(
\mathbf M_a^0
+\sum_{h\in\mathcal H(a)}
\omega_{a|h}\mathbf M_h
\right)\mathbf U_a
=
\mathbf b_a^0
+\sum_{h\in\mathcal H(a)}
\omega_{a|h}\mathbf b_h
+\mathbf B_a.
}
$$

该方程不含相邻 master 速度。一个 master 邻接多个 hanging node 时，只需逐个累加各自贡献，仍只求解一个局部 $2\times2$ 系统。

若 $\mathbf M_a^0$ 在边界约束后正定，则加入非负权重乘以半正定矩阵不会破坏正定性。若原矩阵仅半正定，则仍需通过边界条件或额外几何方向保证可逆性。

---

## 7. 单元动量与全局动量守恒

### 7.1 单元动量方程

单元更新统一使用物理局部合力：

$$
\boxed{
\frac{d(m_c\mathbf U_c)}{dt}
=-
\sum_{a\in\mathcal A(c)}\mathbf F_{ca}^{\Pi}
-
\sum_{h\in\mathcal H(c)}\mathbf F_{ch}^{\mathrm{phys}}.
}
$$

不得把 $\mathbf Q_{ch\to a}^{\mathrm D}$ 和 $\mathbf Q_{ch\to b}^{\mathrm D}$ 再作为两个物理角点力重复写入单元动量。

### 7.2 全局动量定理

定义

$$
\mathbf F_a^{\Pi}
=\sum_{c\in\mathcal C_0(a)}\mathbf F_{ca}^{\Pi},
\qquad
\mathbf F_{h,a}
=\sum_{c\in\mathcal C(h)}\mathbf F_{ch,a}.
$$

对所有 master 平衡求和，并利用

$$
\omega_a\mathbf F_{h,a}
+\omega_b\mathbf F_{h,b}
=\sum_{c\in\mathcal C(h)}\mathbf F_{ch}^{\mathrm{phys}},
$$

得到

$$
\boxed{
\frac{d}{dt}\sum_cm_c\mathbf U_c
=\sum_{a\in\partial\Omega}\mathbf B_a.
}
$$

因此：

- 无外力封闭系统下总动量保持不变；
- 周期边界下，成对广义力相反，净外力为零；
- 固定壁或滑移壁一般可施加非零反力，流体总动量未必保持不变；
- 若边界物理力作用在 hanging node，应按运动学权重映射到主点，并同时保证合力和边界功一致。

---

## 8. 共轭支路功与总能量守恒

### 8.1 悬点支路广义力功

DBGF 使用其实际注入 master 方程的广义力及共轭速度计算功：

$$
\boxed{
\mathcal W_{ch}^{\mathrm D}
=\omega_a\mathbf U_a\cdot\mathbf F_{ch,a}
+\omega_b\mathbf U_b\cdot\mathbf F_{ch,b}.
}
$$

中点情形为

$$
\mathcal W_{ch}^{\mathrm D}
=\frac12\mathbf U_a\cdot\mathbf F_{ch,a}
+\frac12\mathbf U_b\cdot\mathbf F_{ch,b}.
$$

即使 $h$ 是单元 $c$ 的一个角，$h$ 的两个父自由度仍是 $a,b$，所以该单元的悬点功必须同时包含 $\mathbf U_a$ 和 $\mathbf U_b$。

### 8.2 单元总能量方程

$$
\boxed{
\begin{aligned}
\frac{d(m_cE_c)}{dt}
=&-
\sum_{a\in\mathcal A(c)}
\mathbf F_{ca}^{\Pi}\cdot\mathbf U_a\\
&-
\sum_{h\in\mathcal H(c)}
\left[
\omega_a\mathbf F_{ch,a}\cdot\mathbf U_a
+\omega_b\mathbf F_{ch,b}\cdot\mathbf U_b
\right].
\end{aligned}
}
$$

动量和能量必须使用同一批局部支路力、权重、阻抗、几何量和时间阶段。

### 8.3 全局总能量定理

对所有单元求和并按 master 重组：

$$
\begin{aligned}
\frac{d}{dt}\sum_cm_cE_c
&=-\sum_a\mathbf U_a\cdot
\left(
\mathbf F_a^{\Pi}
+\sum_{h\in\mathcal H(a)}
\omega_{a|h}\mathbf F_{h,a}
\right)\\
&=\boxed{
\sum_{a\in\partial\Omega}
\mathbf U_a\cdot\mathbf B_a}.
\end{aligned}
$$

因此，无外力封闭系统和周期系统保持总能量；固定壁或纯法向反力的滑移壁满足零边界功时，总能量也保持不变，但流体总动量仍可因壁面反力而改变。

### 8.4 与 GCL 相容的内能方程

令

$$
\delta\mathbf U_a=\mathbf U_a-\mathbf U_c,
\qquad
\delta\mathbf U_b=\mathbf U_b-\mathbf U_c,
\qquad
\delta\mathbf U_h=\mathbf U_h-\mathbf U_c.
$$

从总能量方程减去 $\mathbf U_c$ 点乘单元动量方程，得到

$$
\boxed{
\begin{aligned}
m_c\frac{de_c}{dt}
=&-P_c\frac{dV_c}{dt}\\
&+\sum_{a\in\mathcal A(c)}
\delta\mathbf U_a^T\mathbf M_{ca}\delta\mathbf U_a\\
&+\sum_{h\in\mathcal H(c)}
\mathcal R_{ch}^{\mathrm D},
\end{aligned}
}
$$

其中

$$
\boxed{
\mathcal R_{ch}^{\mathrm D}
=\omega_a\delta\mathbf U_a^T
\mathbf M_{ch}\delta\mathbf U_a
+\omega_b\delta\mathbf U_b^T
\mathbf M_{ch}\delta\mathbf U_b
\ge0.
}
$$

关键压力项满足

$$
P_c\mathbf N_{ch}\cdot
(\omega_a\mathbf U_a+\omega_b\mathbf U_b)
=P_c\mathbf N_{ch}\cdot\mathbf U_h.
$$

因此 DBGF 的可逆压力功与使用 $\mathbf U_h$ 的体积/GCL 离散严格相容。非负半离散耗散不自动保证有限时间步下的内能或压力正性。

---

## 9. 相对速度耗散及其解释

物理悬点功定义为

$$
\mathcal W_{ch}^{\mathrm{phys}}
=\mathbf U_h\cdot\mathbf F_{ch}^{\mathrm{phys}}.
$$

直接展开得到

$$
\boxed{
\begin{aligned}
\mathcal D_{ch}
&=\mathcal W_{ch}^{\mathrm{phys}}
-\mathcal W_{ch}^{\mathrm D}\\
&=\omega_a\omega_b
(\mathbf U_a-\mathbf U_b)^T
\mathbf M_{ch}
(\mathbf U_a-\mathbf U_b)\ge0.
\end{aligned}
}
$$

中点情形为

$$
\boxed{
\mathcal D_{ch}
=\frac14
(\mathbf U_a-\mathbf U_b)^T
\mathbf M_{ch}
(\mathbf U_a-\mathbf U_b).
}
$$

一般多父节点的非负权重推广为

$$
\mathcal D
=\frac12\sum_{i,j}\omega_i\omega_j
(\mathbf U_i-\mathbf U_j)^T
\mathbf M
(\mathbf U_i-\mathbf U_j).
$$

$\mathcal D_{ch}$ 应解释为 DBGF 相对于严格物理悬点功引入的**主点相对速度耗散**，而不是舍入误差。它在 $\mathbf U_a=\mathbf U_b$ 时消失；若 $\mathbf M_{ch}$ 有零空间，则某些不被半边法向感知的速度差也不会产生该耗散。

定义物理悬点耗散

$$
\mathcal R_{ch}^{\mathrm{phys}}
=(\mathbf U_h-\mathbf U_c)^T
\mathbf M_{ch}
(\mathbf U_h-\mathbf U_c),
$$

则有加权方差恒等式

$$
\boxed{
\mathcal R_{ch}^{\mathrm D}
=\mathcal R_{ch}^{\mathrm{phys}}
+\mathcal D_{ch}.
}
$$

对光滑速度场，$\mathbf U_a-\mathbf U_b=O(h)$。在 $d$ 维中若 $\mathbf M_{ch}=O(h^{d-1})$，则单个界面角的 $\mathcal D_{ch}=O(h^{d+1})$；固定光滑粗细界面上的累计量预期为 $O(h^2)$。该尺度判断需要在论文中结合实际 AMR 界面计数和网格规则给出正式证明或数值验证。

---

## 10. 半离散算法流程

### 10.1 局部构造

```text
对每个物理 cell owner：
1. 构造所有直接 master 角点的 M_ca、b_ca；
2. 构造所有 hanging 角点的局部 M_ch、b_ch；
3. 保存半边几何、父主点 a/b、固定权重和 cell owner；
4. 检查完整单元几何闭合。
```

### 10.2 悬点聚合与 master 组装

```text
1. 聚合直接 master 量 M_a^0、b_a^0；
2. 对每个 h 唯一聚合 M_h、b_h；
3. 向端点 a 注入 omega_a M_h、omega_a b_h；
4. 向端点 b 注入 omega_b M_h、omega_b b_h；
5. 每个 h 对每个端点只注入一次；
6. 不建立 a-b 非对角块。
```

### 10.3 Master 求解和悬点恢复

```text
1. 施加边界约束或功共轭边界力；
2. 每个 master 独立求解一个 2x2 系统；
3. 检查条件数、有限性和节点残差；
4. owner -> ghost 交换 master 速度；
5. 恢复 U_h=omega_a U_a+omega_b U_b；
6. cell owner 用局部 M_ch、b_ch 恢复 F_ch,a、F_ch,b；
7. 构造 F_ch^phys=omega_a F_ch,a+omega_b F_ch,b。
```

### 10.4 几何、动量和能量更新

```text
1. 几何和体积使用 U_a、U_h；
2. 单元动量使用直接角点力和局部 F_ch^phys；
3. 单元总能量使用直接角点功和 DBGF 支路广义力功；
4. 不再添加 D_ch；
5. 不叠加旧 lambda_h 修正；
6. 更新后执行 EOS、体积和热力学正性检查。
```

### 10.5 MPI 唯一所有权

- cell owner 计算并保存局部 $\mathbf M_{ch},\mathbf b_{ch}$；
- hanging owner 唯一归约 $\mathbf M_h,\mathbf b_h$；
- hanging owner 向每个 master owner 只发送一次权重贡献；
- master 速度交换完成后，cell owner 才恢复局部支路力；
- ghost 单元不得写动量、功或耗散；
- 聚合量不得回写为单元通量；
- 所有量必须位于同一 Runge-Kutta stage 或时间中心。

---

## 11. 与旧 hanging 修正路径的互斥性

旧方案若使用

$$
\boldsymbol\lambda_h
=\mathbf M_h\mathbf U_h-\mathbf b_h
=-\mathbf F_h^{\mathrm{phys}},
$$

并把 $\alpha_{ch}\boldsymbol\lambda_h$ 分配给邻接单元，则与本文格式互斥。DBGF 已经在 master 系统中包含 hanging 作用，并已恢复局部物理合力用于单元动量。

严禁：

- 同时加入新支路凝聚和旧 $\boldsymbol\lambda_h$ 修正；
- 把两个支路广义力再次作为两个物理力写入单元动量；
- 用聚合力 $\mathbf F_h$ 更新每个邻接单元；
- 在 DBGF 能量方程上再次添加 $\mathcal D_{ch}$；
- 在新 master 解上临时叠加旧修正作为 fallback。

若门禁失败并回退旧方案，必须撤销新方案对 master 系统的全部贡献并重新组装、重新求解。

---

# 第二部分：两种参照表述

## 12. 物理悬点功闭合表示

### 12.1 能量方程

保持 DBGF 的解耦 master 动量方程不变，但把单元悬点能量项写成物理悬点功：

$$
\boxed{
\begin{aligned}
\frac{d(m_cE_c)}{dt}
=&-
\sum_{a\in\mathcal A(c)}
\mathbf F_{ca}^{\Pi}\cdot\mathbf U_a\\
&-
\sum_{h\in\mathcal H(c)}
\mathbf F_{ch}^{\mathrm{phys}}\cdot\mathbf U_h
+\sum_{h\in\mathcal H(c)}S_{ch}.
\end{aligned}
}
$$

由于

$$
-\mathcal W_{ch}^{\mathrm{phys}}
+\mathcal D_{ch}
=-\mathcal W_{ch}^{\mathrm D},
$$

取

$$
\boxed{S_{ch}=\mathcal D_{ch}}
$$

时，该表示与 DBGF 对每个单元-悬点对的半离散总能量和内能更新完全相同。它不是新的数值算法，而是把 DBGF 隐含的相对速度耗散显式记账。

全局求和得到

$$
\boxed{
\frac{d}{dt}\sum_cm_cE_c
=\sum_{a\in\partial\Omega}\mathbf U_a\cdot\mathbf B_a
+\sum_{c,h}(S_{ch}-\mathcal D_{ch}).
}
$$

因此，不加 $S_{ch}$ 时物理悬点功与 DBGF 的解耦动量方程并不功共轭；局部取 $S_{ch}=\mathcal D_{ch}$ 才恢复同一守恒更新。

这种等价要求物理功和 $\mathcal D_{ch}$ 使用同一阶段的 $\mathbf M_{ch},\mathbf b_{ch},\omega,\mathbf U_a,\mathbf U_b$，且补偿未经时间滞后、限幅或重复写入。

### 12.2 跨单元重分配不是等价表述

定义

$$
\mathcal D_h
=\sum_{c\in\mathcal C(h)}\mathcal D_{ch}.
$$

若取

$$
S_{ch}=\beta_{ch}\mathcal D_h,
\qquad
\beta_{ch}\ge0,
\qquad
\sum_{c\in\mathcal C(h)}\beta_{ch}=1,
$$

则每个悬点邻域及全局总能量仍闭合，但单元能量差为

$$
\Delta\dot E_c
=\sum_{h\in\mathcal H(c)}
(\beta_{ch}\mathcal D_h-\mathcal D_{ch}).
$$

因此该变体不再与 DBGF 逐单元等价。局部内能、温度和压力会不同，并在后续阶段反馈到阻抗和动量更新。第一版实现和论文主方法不建议采用该变体。

---

## 13. 约束虚功耦合格式

### 13.1 约束力的严格映射

若只采用“悬点没有独立自由度”这一运动学假设，则允许虚位移满足

$$
\delta\mathbf x_h
=\omega_a\delta\mathbf x_a
+\omega_b\delta\mathbf x_b.
$$

唯一物理悬点力的虚功为

$$
\mathbf F_{ch}^{\mathrm{phys}}\cdot\delta\mathbf x_h
=\omega_a\mathbf F_{ch}^{\mathrm{phys}}\cdot\delta\mathbf x_a
+\omega_b\mathbf F_{ch}^{\mathrm{phys}}\cdot\delta\mathbf x_b.
$$

因此严格约束虚功广义力为

$$
\boxed{
\mathbf Q_{ch\to a}^{\mathrm V}
=\omega_a\mathbf F_{ch}^{\mathrm{phys}},
\qquad
\mathbf Q_{ch\to b}^{\mathrm V}
=\omega_b\mathbf F_{ch}^{\mathrm{phys}}.
}
$$

它们满足

$$
\mathbf Q_{ch\to a}^{\mathrm V}
+\mathbf Q_{ch\to b}^{\mathrm V}
=\mathbf F_{ch}^{\mathrm{phys}},
$$

$$
\mathbf U_a\cdot\mathbf Q_{ch\to a}^{\mathrm V}
+\mathbf U_b\cdot\mathbf Q_{ch\to b}^{\mathrm V}
=\mathbf U_h\cdot\mathbf F_{ch}^{\mathrm{phys}}.
$$

所以在相同的几何、边界和时间阶段假设下，该格式不需要 $\mathcal D_{ch}$ 即可在半离散层面同时满足物理虚功、动量守恒、总能量守恒和 GCL。

### 13.2 主点耦合是约束消元的必然结果

令 $b(h)$ 表示悬点 $h$ 相对于当前主点 $a$ 的另一个父主点。由于

$$
\mathbf F_h^{\mathrm{phys}}
=\mathbf b_h
-\mathbf M_h\left(
\omega_{a|h}\mathbf U_a
+\omega_{b(h)|h}\mathbf U_{b(h)}
\right),
$$

$a$ 点方程包含

$$
\boxed{
\left(
\mathbf M_a^0
+\sum_{h\in\mathcal H(a)}\omega_{a|h}^2\mathbf M_h
\right)\mathbf U_a
+\sum_{h\in\mathcal H(a)}
\omega_{a|h}\omega_{b(h)|h}
\mathbf M_h\mathbf U_{b(h)}
=\mathbf b_a^0
+\sum_{h\in\mathcal H(a)}\omega_{a|h}\mathbf b_h
+\mathbf B_a.
}
$$

每个悬点产生的两主点矩阵块为

$$
\boxed{
\begin{bmatrix}
\omega_a^2\mathbf M_h & \omega_a\omega_b\mathbf M_h\\
\omega_a\omega_b\mathbf M_h & \omega_b^2\mathbf M_h
\end{bmatrix},
}
$$

它对称半正定，但包含非对角块。因此“悬点无独立自由度”不意味着两个父主点可以独立求解；严格约束消元会把耦合传递给父自由度。

### 13.3 DBGF 相对于约束虚功的变化

在同一给定局部状态和同一组主点速度下，两种广义力之差为

$$
\boxed{
\begin{aligned}
\mathbf Q_{ch\to a}^{\mathrm D}
-\mathbf Q_{ch\to a}^{\mathrm V}
&=-\omega_a\omega_b\mathbf M_{ch}
(\mathbf U_a-\mathbf U_b),\\
\mathbf Q_{ch\to b}^{\mathrm D}
-\mathbf Q_{ch\to b}^{\mathrm V}
&=+\omega_a\omega_b\mathbf M_{ch}
(\mathbf U_a-\mathbf U_b).
\end{aligned}
}
$$

这是一对合力为零的附加广义力，所以不改变物理合力；其总功为

$$
\boxed{
\mathbf U_a\cdot
(\mathbf Q_{ch\to a}^{\mathrm D}-\mathbf Q_{ch\to a}^{\mathrm V})
+\mathbf U_b\cdot
(\mathbf Q_{ch\to b}^{\mathrm D}-\mathbf Q_{ch\to b}^{\mathrm V})
=-\mathcal D_{ch}.
}
$$

因此 DBGF 可以解释为：在严格约束虚功格式上加入一个抑制父主点相对运动的耗散广义力对，以换取 master 系统完全解耦。

---

## 14. 三种表述的关系和方法选择

| 项目 | 解耦支路广义力 DBGF | 物理悬点功闭合表示 | 约束虚功耦合格式 |
|---|---|---|---|
| 悬点独立自由度 | 无 | 无 | 无 |
| Master 系统 | 独立 $2\times2$ | 与 DBGF 相同 | 存在 $a-b$ 非对角块 |
| Master 广义力 | 各自速度支路 | 与 DBGF 相同 | 约束转置映射物理力 |
| 单元动量力 | $\mathbf F_{ch}^{\mathrm{phys}}$ | $\mathbf F_{ch}^{\mathrm{phys}}$ | $\mathbf F_{ch}^{\mathrm{phys}}$ |
| 单元方程中的悬点能量项 | $-\mathcal W_{ch}^{\mathrm D}$ | $-\mathcal W_{ch}^{\mathrm{phys}}+\mathcal D_{ch}$ | $-\mathcal W_{ch}^{\mathrm{phys}}$ |
| 显式闭合项 | 无 | 局部 $+\mathcal D_{ch}$ | 无 |
| 相对速度耗散 | 隐含 | 显式，数值更新相同 | 无该附加项 |
| 半离散动量守恒 | 是 | 是 | 是 |
| 半离散总能量守恒 | 是 | 局部闭合后是 | 是 |
| GCL | 使用 $\mathbf U_h$ | 使用 $\mathbf U_h$ | 使用 $\mathbf U_h$ |
| 主要用途 | 本文主方法与首版实现 | 诊断/等价记账 | 理论参照或耦合求解器 |

若“master 必须独立求解”是不可放弃的工程约束，DBGF 是三者中最直接的守恒闭合。若优先要求严格物理约束虚功，应采用耦合格式，而不能同时宣称 master 完全解耦。

---

## 15. 实现门禁与验证

### 15.1 逐半边和局部恒等式

检查

$$
r_{\pi,e}
=\left|
\pi_{ch,e}^{\mathrm{phys}}
-\omega_a\pi_{ch,e|a}
-\omega_b\pi_{ch,e|b}
\right|,
$$

$$
r_{F,ch}
=\left\|
\mathbf F_{ch}^{\mathrm{phys}}
-\omega_a\mathbf F_{ch,a}
-\omega_b\mathbf F_{ch,b}
\right\|,
$$

$$
r_{D,ch}
=\left|
\mathcal W_{ch}^{\mathrm{phys}}
-\mathcal W_{ch}^{\mathrm D}
-\mathcal D_{ch}
\right|.
$$

所有残差应为可解释的舍入量级，并检查 $\mathcal D_{ch}\ge-\varepsilon_D$。

### 15.2 Master 残差

令增强系统为 $\widehat{\mathbf M}_a\mathbf U_a=\widehat{\mathbf b}_a$，检查

$$
r_a
=\frac{
\|\widehat{\mathbf M}_a\mathbf U_a-\widehat{\mathbf b}_a\|
}{
\|\widehat{\mathbf M}_a\|\,\|\mathbf U_a\|
+\|\widehat{\mathbf b}_a\|+\varepsilon
}.
$$

### 15.3 完全离散 GCL

若采用阶段值离散，应检查

$$
V_c^{n+1}-V_c^n
\stackrel{?}{=}
\Delta t
\left[
\sum_a\mathbf N_{ca}^{*}\cdot\mathbf U_a^{*}
+\sum_h\mathbf N_{ch}^{*}\cdot\mathbf U_h^{*}
\right].
$$

若体积由新坐标重算，应使用与时间积分匹配的 swept-volume 恒等式，而不能仅验证半离散公式。

### 15.4 全局守恒和边界功

检查

$$
\Delta\sum_cm_c\mathbf U_c
-\int_{t^n}^{t^{n+1}}\sum_a\mathbf B_a\,dt,
$$

$$
\Delta\sum_cm_cE_c
-\int_{t^n}^{t^{n+1}}
\sum_a\mathbf U_a\cdot\mathbf B_a\,dt.
$$

周期边界必须同时验证成对力和成对功抵消；固定壁应分别报告非零动量交换和零边界功。

### 15.5 最低测试集合

- 均匀平移、均匀压力和 Galilean 平移不变性；
- 均匀压缩/膨胀及线性速度场 GCL；
- 无 hanging 时退化为 regular solver；
- $\mathbf U_a=\mathbf U_b$ 时 $\mathcal D_{ch}=0$；
- 法向与切向主点速度差；
- 一个 master 邻接多个 hanging node；
- hanging edge 穿越 MPI 分区；
- 声波和涡旋跨越粗细界面的光滑收敛；
- Sod、Sedov 等含激波粗细界面测试；
- 强膨胀和近真空下的 $V_c,\rho_c,e_c,P_c$ 正性；
- AMR 加密、粗化和守恒重映射前后的残差；
- DBGF 与约束虚功耦合格式的定量对照。

---

## 16. 面向代码实现和 JCP/SIAM 论文的建议

### 16.1 代码实现建议

1. **先实现唯一主方法**：首版只实现 DBGF 共轭支路功，不同时保留物理功闭合和跨单元重分配分支。
2. **局部量类型化**：把 `CellHangingContribution` 与 `AggregatedHangingContribution` 设计为不同数据结构，避免 $\mathbf M_{ch}$ 与 $\mathbf M_h$ 混用。
3. **力的命名区分物理含义**：支路量命名为 `branch_force_a/b`，合力命名为 `physical_corner_force`；不要把二者都命名为 `hanging_force`。
4. **共享阶段缓存**：动量、能量和诊断复用同一阶段的 $\mathbf M_{ch},\mathbf b_{ch},\mathbf F_{ch,a/b}$，避免因重新计算阻抗产生不一致。
5. **把恒等式做成运行时门禁**：在 debug/verification 模式中检查压力拆分、力恢复、$\mathcal D_{ch}$、master 残差、GCL 和全局守恒。
6. **完全移除旧路径而非叠加开关**：旧 $\boldsymbol\lambda_h$ 修正必须与新凝聚路径互斥；fallback 必须触发重新组装。
7. **总能量优先**：优先直接推进守恒总能量。若代码推进内能，必须另行推导完全离散动能交换，不能只引用半离散链式法则。
8. **保留约束虚功参照求解器**：即使不用于生产计算，也建议实现一个小规模耦合版本，用于验证 DBGF 的附加耗散和界面误差。

### 16.2 论文中的方法定位

论文不宜把 DBGF 表述为严格约束虚功离散。更准确的定位是：

> 一种保持物理悬点合力、局部 master 可解性、半离散总能量守恒和 GCL 的解耦广义力构造；其相对于严格约束虚功格式的差异是一对合力为零、功率为 $-\mathcal D_{ch}$ 的相对速度耗散力。

建议避免使用“能量误差修正”描述 $\mathcal D_{ch}$。论文中应称其为 relative-master-velocity dissipation、decoupling dissipation 或 power representation gap，并明确它是方法设计的可分析性质。

### 16.3 建议组织成正式命题

论文离散理论部分至少应包含以下可独立引用的命题：

1. **仿射压力拆分引理**：逐半边证明物理悬点压力等于两个冻结阻抗支路压力的加权和；
2. **物理合力恢复引理**：证明局部支路广义力之和等于物理悬点角点力；
3. **解耦性命题**：给出 master 局部矩阵并证明不含相邻 master 速度；
4. **全局动量定理**：包含无外力、周期和一般边界广义力三种情况；
5. **总能量定理**：明确边界功符号及零功固壁条件；
6. **GCL 相容性命题**：证明支路压力的可逆功恢复 $P_c\,dV_c/dt$；
7. **耗散命题**：证明 $\mathcal R_{ch}^{\mathrm D}\ge0$ 以及 $\mathcal R_{ch}^{\mathrm D}=\mathcal R_{ch}^{\mathrm{phys}}+\mathcal D_{ch}$；
8. **与约束虚功格式的比较命题**：给出两种广义力之差及其零合力、负功性质；
9. **完全离散守恒定理**：针对最终选定的时间积分和 swept-volume 公式重新证明，而不是只引用半离散结果。

### 16.4 JCP/SIAM 审稿中最可能被追问的问题

- DBGF 在光滑解上的一致性阶是多少，$\mathcal D_{ch}$ 是否随网格加密充分消失？
- 解耦附加耗散是否导致粗细界面的声波反射、涡量衰减或过度加热？
- 与严格约束虚功耦合格式相比，精度损失换来了多少计算和通信收益？
- 熵稳定性是总内能非负耗散，还是对具体 EOS 的离散熵不等式？
- 完全离散总能量、GCL 和正性是否能同时满足？
- AMR 加密、粗化、重映射和 MPI owner/ghost 路径是否保持唯一守恒写入？
- 非线性 Riemann 求解器或速度相关阻抗能否保持支路仿射拆分？若不能，推广方式是什么？

这些问题不应只放在“未来工作”中。至少应给出理论限制、数值证据和失败模式。

### 16.5 建议的数值证据结构

面向 JCP 或 SIAM Journal on Scientific Computing，建议按以下层次组织数值部分：

1. 机器精度恒等式和串并行一致性；
2. 制造解或光滑声波给出空间/时间收敛阶；
3. 粗细界面上的透射、反射和 $\mathcal D_{ch}$ 网格尺度；
4. DBGF、约束虚功耦合格式和基准 conforming 网格三方对照；
5. 强激波、近真空和大变形下的鲁棒性；
6. AMR 动态加密/粗化下的守恒和 GCL；
7. 规模、迭代次数、通信量和 wall-clock 成本，量化“解耦”的实际收益。

只有守恒证明通常不足以支撑 JCP/SIAM 论文。主创新应落在“约束悬点物理合力的可恢复解耦构造、严格的能量/GCL 闭合、可量化的解耦耗散及其精度-效率权衡”这一完整论证链上。

---

## 17. 推荐结论

首版代码和论文主方法建议采用 **解耦支路广义力格式 DBGF**，原因是它同时满足本文设定的两个设计约束：悬点无独立自由度，且 master 保持独立局部求解。

物理悬点功闭合表示只作为等价诊断形式，不作为独立算法；约束虚功耦合格式作为理论基准，用于说明严格物理约束映射为何产生耦合，并定量评估 DBGF 为解耦引入的相对速度耗散。

最终论文必须把以下边界说清楚：DBGF 是一个守恒、GCL 相容、带可证明非负解耦耗散的离散广义力方法；它不是严格约束虚功格式，也不应在没有完全离散分析和收敛证据时扩大声称范围。
