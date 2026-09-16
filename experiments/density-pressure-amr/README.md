# 密度—压力联合识别 AMR：可复算试验包

本包保存 2026-09-16 确认的 Noh / Sedov 候选实现、成功配置和回归依据。数学物理思想、公式及流程见根 README 第 10.3 节：密度发现结构、压力确认、压缩辅助，配合家族减疏及一致性保护。

## 范围

- 这是独立源码快照，不是正式 src/ 的替换；正式工作区已有修改不随本包提交。
- src/ 是历史 bin-front-timing 构建所需的 64 个项目源码依赖，不依赖未跟踪的正式源码。移除了该构建之后新增、尚未测试的时间连续确认分支。
- 为保持已验证计算路径，保留成功构建依赖的诊断代码及关闭的历史实验入口，不将它们作为新的推荐算法。不要自行启用 presets.json 以外的开关。
- 不提交 exe、对象文件、VTU、PLT、大型日志或其他参数扫描目录。提交期复算摘要见 verification.json，历史期望值见 expected-results.json。
- 本地 Windows / MSYS2 UCRT64 + MS-MPI、MPI1 复算；不宣称完成 Linux、MPI 多进程或性能验证。

## 成功配置

| 项目 | NohCartesian | SedovCartesian |
|---|---|---|
| 目标时间 | 0.6 | 1.0 |
| 层级 / AMR 间隔 | L4–L7 / 每4步 | L4–L7 / 每4步 |
| 密度加密 / 减疏 | 0.20 / 0.19 | 0.20 / 0.19 |
| 压力归一化 | 全局尺度，alpha=1 | 局部尺度，alpha=0 |
| 压力加密 / 减疏 | 0.20 / 0.10 | 0.05 / 0.025 |
| 压缩加密 / 阻止减疏 | >1 / >=0.5 | >1 / >=0.5 |
| 历史终态单元数 | 1081 | 1906 |
| 历史原始最大密度 | 19.4650581503 | 6.0249058675 |

计算结束时间会略超过目标。这里保留未经删峰或平滑的全域最大密度；这些数值不是解析参考值。

两个 ini 只包含程序参数；configs/presets.json 还记录必需环境开关，不能只复制 ini 后使用正式 bin 运行。共同开启压缩辅助、家族拓扑预演、同轮保护、派生状态同步和内能修正权重。耗散权重 mode=1、overlap-remap mode=3 仅审计，不应用其候选修正。Noh 的解析前沿相关输出只作诊断，不用于标记加密。

## Common-weights AMR 扩展（2026-09-16）

本包新增可选 CW-AMR 线性共享平滑指标，关闭 `AMR_WENO_SENSOR` 时仍保留原密度—压力判据。根目录 `README.md` 第 10.4 节记录核心思想、数学公式、算法流程及 Noh/Sedov/SodCartesian 的 L5–L8 测试摘要；[详细数学与运行说明](tests/LINEAR-SERVICE.md) 提供新路线复算命令。下述 `run.ps1` 仍用于原 L4–L7 归档配置；新路线使用 `tests/run_service_study.py`，不替换原参考哈希。

## 编译与复算

在仓库根目录，使用现有 Makefile 与已安装的 p4est、libsc、zlib、MS-MPI 依赖执行：

    & ./experiments/density-pressure-amr/build.ps1 -Jobs 3
    & ./experiments/density-pressure-amr/run.ps1 -Case noh
    & ./experiments/density-pressure-amr/run.ps1 -Case sedov

构建输出位于本包 bin/ 和 build/，不会覆盖根 bin/。每次运行在本包 runs/ 新建目录；可用 -RunName 指定名称，已存在的目录一律拒绝覆盖。结果包括 DistanceProfiles.plt、EnergyError.plt、output/、solver.log、solver.err、参数、环境与 verification.json。

运行脚本清理继承的 AMR_* 环境变量，再加载固定预设，结束后恢复环境；因为某些开关按“是否存在”判断，不能一律用值 0 表示关闭。脚本检查正常退出、状态检查启用、接受步数/时间、能量记录及最终 PLT 的精确 SHA256。哈希不同即失败，不自动替换历史参考；不同编译器或平台的浮点差异应另行审查。

编译保留既有警告（包括非 void 函数返回路径、可能未初始化变量及实验诊断格式警告），本提交没有为消除警告改变公式。测试通过不代表这些潜在风险已经修复。

## 数学方法实现入口

- src/amr/least_squares_gradient.h、density_gradient.h：面邻居加权最小二乘与压力指标。
- src/amr/amr_criteria.h、pressure_experiment.h、compression_experiment.h：加密与家族减疏逻辑。
- src/amr/same_pass_experiment.h、coarsen_plan_experiment.h：同轮与拓扑往返保护。
- src/diagnostics/derived_state_experiment.h：压力、声速等派生状态同步。
- src/hydro/hydro_callbacks.h：内能权重的悬挂点修正分配。

## 解释边界

这是一组允许少量残余噪声的成功完成候选，不是跨问题通用黄金门禁。压力 AND 可能降低近等压接触结构的分辨率；Sod 对照已有漏加密，未纳入成功配置。不得只凭单元数减少宣称精度或速度改善，也不由能量误差小推出局部保正性或熵稳定性。

原研究目录 .tmp/noh-sensor-study-20260914/ 保留详细诊断及全部结果，但复算本包不依赖该临时目录。源码清单及来源见 source-manifest.json。
