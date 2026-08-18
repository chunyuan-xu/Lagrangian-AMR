# Sedov-AMR 效率收益典型算例

本算例用于评估 Lagrangian-AMR 求解器的**效率收益与网格集中能力**：在二维 Sedov 爆炸波传播至 `t=1` 的过程中，用解析激波轨迹驱动 Distance AMR，使最高层网格集中在激波附近，并记录 wall-clock、网格规模和输出序列。

> 本算例是性能/行为基准，不是新的黄金参考资产；当前 canonical G1/G3 仍以 [`golden-gates.md`](golden-gates.md) 及 `reference/` 为准。

## 固定配置

仓库根目录 [`../param.ini`](../param.ini) 默认保存本配置：

```ini
which_case = 1
start_time = 0.0
end_time = 1.0
minus_level = 4
max_level = 7
max_time_step = 100000
refine_coarsen_enum = 5

distance_shock_radius_scale = 1.0
distance_shock_radius_exponent = 0.5
distance_band_half_width = 0.04

refine_coarsen_time = 0.0
refine_period = 1
write_interval_time = 0.05
write_interval_step = 200000
```

二维 Sedov 自相似激波位置与瞬时速度为：

\[
R_s(t)=\sqrt{t},\qquad D_s(t)=\frac{1}{2\sqrt{t}}.
\]

Distance AMR 物理环带为：

\[
|r-R_s(t)|\le 0.04.
\]

在 `t≈1` 时，名义环带约为 `[0.96, 1.04]`。

## 运行

PowerShell：

```powershell
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
$env:TEMP = "$PWD\.tmp"
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null

make -j8
& .\bin\AMR_Solver.exe
```

运行前应清空或隔离 `output/`，避免旧帧混入本次统计。输出按约 0.05 的物理时间间隔写入 `.pvtu`/`.vtu`；最终帧通常为 `output/p4est_Lagrangian_4617.pvtu`。

## 当前串行基线

测试机器：Intel Core i5-10210U（4 物理核 / 8 逻辑处理器），单进程运行。

| 指标 | 当前基线 |
|---|---:|
| wall-clock | 50.8 s |
| 最终输出时刻 | 0.999558338 |
| 最终求解器时刻 | 1.000568 |
| 输出帧 | 21 |
| 总 cell | 3,496 |
| L4 / L5 / L6 / L7 | 149 / 164 / 347 / 2,836 |
| 最终总能量相对误差 | ~`2.0e-14` |

最终物理环带：

\[
[0.959779, 1.039779].
\]

末帧中 2,598 个 L7 单元与物理环带相交；另有 238 个 L7 作为有限单元尺寸/2:1 平衡 halo。不存在四个子单元都完全位于物理环带外、仍可继续粗化的完整 L7 sibling family。

## 如何正确查看物理网格

VTU 的默认 `Position` 是 p4est 逻辑参考坐标；真正随流体运动的拉格朗日物理坐标是 `NodeX` / `NodeY`。在 ParaView 中查看物理网格：

1. 打开 `.pvtu`；
2. 添加 **Calculator**；
3. 表达式设为 `NodeX*iHat + NodeY*jHat`；
4. 勾选 **Coordinate Results**；
5. 按 `level` 着色。

若版本没有 `Coordinate Results`，计算 `(NodeX-coordsX)*iHat + (NodeY-coordsY)*jHat`，再应用 **Warp By Vector**（Scale Factor = 1）。禁止用逻辑 `Position` 的半径判断 Distance AMR 是否过度加密。

## 效率比较纪律

比较优化前后或串行/MPI 版本时必须固定：

- 同一提交（除待比较改动）；
- 同一 `param.ini`；
- 同一硬件、编译器与 Release flags；
- 从干净 `output/` 开始；
- 调试环境变量全部关闭；
- 同时报告 wall-clock、最终 cell 分层、求解器退出码、物理阵面位置和总能量误差。

性能提升不能以数值失稳、物理环带漏加密、黄金门禁失败或放宽比较容差为代价。
