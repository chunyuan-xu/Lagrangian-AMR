---
name: lagrangian-amr-workflow
description: >
  Complete project workflow skill for the Lagrangian-AMR solver (C:\Lagrangian-AMR).
  Covers build toolchain, regression validation, MPI parallel debugging, mathematical
  physics derivation, codebase architecture analysis, and project management.
  Load this skill at the start of any new conversation working on this project.
---

# Lagrangian-AMR 项目工作流 Skill

## 项目概况

- **项目路径**: `C:\Lagrangian-AMR`
- **求解器**: 基于 p4est 的 2D Lagrangian 可压缩流体动力学 AMR 求解器
- **论文**: *A Decoupled Kinematic-Constrained Hanging Nodal Solver for Cell-Centered Lagrangian Hydrodynamics on Non-Conforming Quadrilateral Meshes*
- **编译环境**: Windows + MSYS2 UCRT64 + MS-MPI
- **语言**: C++14
- **并行**: MPI (MS-MPI / mpiexec)

---

## 编译工具链

### 必须设置的 PATH
```powershell
$env:PATH = "C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
```

### 编译命令
```powershell
make clean    # 清理
make -j8      # 并行编译（约 30 秒）
```

### 编译产物
- 可执行文件: `bin/AMR_Solver.exe`
- MPI 运行: `mpiexec -n <N> ./bin/AMR_Solver.exe`
- 串行运行: `./bin/AMR_Solver.exe`

---

## 关键文件一览

| 文件 | 用途 |
|---|---|
| `src/main.cpp` | 主求解器（~5400行，Lagrangian时间推进、p4est AMR回调、MPI逻辑） |
| `src/alg.cpp` / `src/alg.h` | 核心算法（几何、物理、线性代数） |
| `src/variable.h` | 关键数据结构（`quad_data_t`、`CVariable`） |
| `src/defines.h` | 全局常量和变量索引 |
| `param.ini` | 运行参数（算例类型、AMR层级、时间设置、输出频率） |
| `validate_current.ps1` | 一键回归验证脚本（编译 + 运行 3 个基准算例 + 结果比对） |
| `compare_vtu.py` | VTU 文件数值比对（容差 1e-12） |
| `reference/` | 基准参考结果（Noh_32x32.vtu、SedovAMR.vtu、SodAMR.vtu） |
| `reference/par4_sod/` | **并行(4核)黄金参考**：Sod AMR 4 核 `p4est_Lagrangian_3046.pvtu` + 4 个 rank 分块 |
| `reference/par4_sedov/` | **并行(4核)黄金参考**：Sedov AMR 4 核 `p4est_Lagrangian_3933.pvtu` + 4 个 rank 分块 |
| `WORKFLOW.md` 或 `task.md` | 当前任务清单和里程碑状态 |

---

## 算例配置 (param.ini 速查)

```ini
which_case = 1    # 0=SedovPolar,1=SedovCartesian,4=NohCartesian,7=SodCartesian
minus_level = 5   # AMR 最低细化层（初始均匀网格 = 2^minus_level × 2^minus_level）
max_level = 7     # AMR 最高细化层
enable_amr = true # AMR 开关
refine_period = 4 # 每 N 步 AMR 一次
end_time = 0.5    # 终止时间
write_interval_time = 0.05  # 输出时间间隔
```

---

## 一键回归验证 [GOLDEN-PASS]

```powershell
powershell -File validate_current.ps1
```

验证内容：
1. `make clean && make` — 重新编译
2. Noh 32x32 均匀串行算例（无 AMR）
3. Sod AMR 串行算例
4. Sedov AMR 串行算例
5. 每个算例与 `reference/` 比对（容差 1e-12）

**通过条件**: 所有算例的输出误差 < 1e-12 → 打印 `ALL REGRESSION TESTS PASSED`

---

## 并行(4核)回退黄金算例 [PAR4-GOLDEN]

除串行三基准外，另有两个 **4 核并行黄金参考**，用于核对 MPI 并行结果合理性与并行回退：

| 算例 | 4 核参考 | 生成配置 | 规模(4核总) vs 串行锚点 |
|---|---|---|---|
| Sod AMR | `reference/par4_sod/p4est_Lagrangian_3046.pvtu` | `which_case=7, end_time=0.2, refine_err=1.0, coarsen_error=0.8, refine_period=4, refine_coarsen_time=0.0001` | 4387 cells (1111/1080/1105/1091) vs 串行 4390 |
| Sedov AMR | `reference/par4_sedov/p4est_Lagrangian_3933.pvtu` | `which_case=1, end_time=0.5, refine_err=1.0, coarsen_error=0.8, refine_period=4, refine_coarsen_time=0.0001` | 5434 cells (1359/1356/1358/1361) vs 串行 5422 |

每个 4 核参考 = 1 个 `.pvtu`（汇总）+ `.visit` + 4 个 rank 分块 `.vtu`（`_0000`~`_0003`），Paraview 直接打开 `.pvtu` 即可查看完整解。

### 如何生成 / 核对（标准动作）
1. **先切黄金配置**（阈值 1.0/0.8、`refine_period=4`、`refine_coarsen_time=0.0001`，并**关闭调试 IO**，见「回退测试第一件事」第 0 步）：
   ```bash
   export PATH="/c/msys64/usr/bin:/c/msys64/ucrt64/bin:$PATH"
   mpiexec -n 4 env -u LAGRANGIAN_TRACE_TARGET -u LAGRANGIAN_TRACE_REFINE \
          -u LAGRANGIAN_VERBOSE_AMR -u LAGRANGIAN_TRACE_CHECKSUM ./bin/AMR_Solver.exe
   ```
2. **合理性判据**（4 核结果应满足，否则需排查）：
   - MPI 退出码 0、无 rank 崩溃；
   - 每步总能量误差 ~`1e-15`（守恒完好）；
   - 4 个 rank 分块 cell 数尽量均衡（分区负载均衡）；
   - 4 核总 cell 数与串行锚点规模基本一致（差几个 cell = MPI 分片下 AMR 边界/粗化拓扑差异，黄金宽松阈值下属正常）。
3. **比对**：`compare_vtu.py` 已支持 `.pvtu` 与 `.vtu` 对齐（用 `Global_SFC_ID` 或几何键）；比对 `output/<末帧>.pvtu`（本次运行）与 `reference/par4_<case>/*.pvtu`（黄金参考）。

### 注意事项
- 4 核参考是**黄金配置（宽松阈值 1.0/0.8）**下的结果；若用调试阈值（0.1/0.05）或 `refine_period=1`，网格会明显更细，与 4 核黄金参考对不上，属配置不一致而非回退失败。
- `refine_coarsen_time` 已可从 ini 读（提交 `4ae893e`），但黄金参考本身是用 `0.0001` 生成的，核对时须保持该值。

---

## ⚠️ 回退测试第一件事：先核对每个算例的基本配置参数 (Rollback Anchor Config Audit)

在做「回退到锚点」的算例测试（如 GOLDEN-PASS 三基准、或任何要和 reference/ 比对锚点的回归）时，**第一件必须做的事不是编译、不是运行，而是先确认每个算例的「基本配置参数」当前值，并保证它们与『上一个回退通过的算例』（即被当作锚点的那个版本）完全一致。**

### 为什么必须先做这一步
- 配置参数直接决定网格形态、AMR 判据、输出步数等信息。**只要有一个参数与锚点版本不同，即使物理求解核心完全正确，最终输出（尤其细胞数/点数）也会和 reference/ 对不上**，从而被误判为「回退失败」。
- 这类配置**不只在 `param.ini` 里**，还可能**藏在代码里**（尚未集成到 .ini）。只盯着 `.ini` 核对会漏掉真正的差异来源。

### 配置参数的可能存放位置（都要核对）
1. **`param.ini`（显式字段）**：`which_case`、`end_time`、`minus_level`、`max_level`、`enable_amr`、`refine_period`、`write_interval_time`、`write_interval_step`、`max_time_step`、`delta_time` 等。
2. **`param.ini` 中可能没有、但影响 AMR 的字段（易漏项）**：
   - `refine_err` / `coarsen_error`（AMR 细/粗阈值，**直接决定 AMR 网格形态与细胞数**）；
   - `refine_coarsen_enum`（用哪种梯度/距离判据，取值决定选 `idCPressureGradient` / `idCDensityGradient` / `Distance` 之一）。
3. **代码里的默认值 / 硬编码（param.ini 未暴露）**：
   - 例如 `defines.h` 里 `refine_coarsen_enum = RefineCriteria::DensityGradient` 等默认值；
   - 求解器内部其它魔法常数、迭代次数、单位、边界条件默认值。
   - **`refine_coarsen_time` 已从源码隐藏改为 ini 可读**（提交 `4ae893e` 后 `defines.h` 的 `load_from_config` 会读它），`_anchor_regression.py` 会显式写 `refine_coarsen_time=0.0001`，回退时无需再手工改源码 + 重编译。

> **本项目真实反例**：GOLDEN-PASS 参考锚点（`reference/SodAMR.vtu`、`reference/SedovAMR.vtu`）是在 **`refine_err=1.0, coarsen_error=0.8`**（宽松阈值）下生成的；而当前 `param.ini` 里是 **`0.1 / 0.05`**（step55 调试阶段被调严）。直接拿当前 .ini 跑回归，Sod/Sedov 的 AMR 网格因阈值不同而明显更细（Sod 从锚点 4390 cells 变成 9253 cells，Sedov 从 5422 变成 6628），`compare_vtu.py` 报 cell 数不匹配。**这不是物理求解没回退，而是配置参数与锚点不一致造成的假性失败。** 无 AMR 的 Noh 不受阈值影响，能逐位一致回退（证明物理核心完好）。

### 标准动作（回退测试启动前）
0. **【先检测/关闭高频调试 I/O】** 在做**任何**回退/长期算例运行前，先排查近期调试遗留的高频 IO 或额外全场遍历，否则一个完整算例会被拖慢数倍甚至数十倍：
   - **排查项**：
     - 每时间步多次的 `StatGlobalFieldChecksum`（每步约 8 次全场扫描 + 8 次 allreduce，3046 步累计 2.4 万行、占运行时间大头）；
     - 高频 `refine_dbg_*` 文件（在 `Lagrangian_replace_quads` 里对每个 child `fopen/fprintf/fclose`，曾造成单次运行写出 877 MB / 936 万行）；
     - 每步多次的额外 `p4est_iterate`（如 `trace_target_snapshot*`，即使回调不写文件也会遍历全场）；
     - 详细 `DEBUG: Entering/Finished ...` 控制台日志。
   - **本项目的关闭开关（环境变量，默认即关闭，无需设置）**：
     ```text
     LAGRANGIAN_TRACE_TARGET      # 目标单元/阶段快照、edge/riemann 定点日志
     LAGRANGIAN_TRACE_REFINE      # refine_dbg_* 高频文件
     LAGRANGIAN_VERBOSE_AMR       # DEBUG Entering/Finished 详细控制台日志
     LAGRANGIAN_TRACE_CHECKSUM    # 每步 SubStep 全场的 checksum 输出
     ```
   - **验证方式**：跑一个短步数（如 4 步）冒烟，确认日志里 `grep -c 'Checksum \[SubStep'` 为 0、`grep -c 'DEBUG: Entering'` 为 0，且不新增 `refine_dbg_*`/`corner2_trace_*`/`riemann_dbg_*` 文件。若仍异常地慢，优先怀疑某处高频 IO 未纳入开关。
   - **本节真实反例**：调试期间把上述 checksum/refine 日志全量留在正式路径，一个正常约 1 分钟的 Sod 算例被拖到约 5 分钟甚至 3 小时，会让人误判为「算例本身很慢」或掩盖真正的回归问题。
1. **从 git 确认锚点配置**：用 `git show <GOLDEN-PASS/anchor commit>:param.ini` 取出锚点版本的 `.ini`，同样地 `git show <anchor commit>:<源码文件>` 核对代码内默认值（如 `defines.h` 的 `refine_coarsen_enum`、`refine_err`/`coarsen_error`、`refine_coarsen_time` 等）。
2. **逐项比对**：把当前 `param.ini` + 当前代码里的相关默认值，与锚点版本逐字段比对，列出差异清单。
3. **有差异就先统一**，再编译/运行/比对。特别留意 `refine_err`、`coarsen_error`、`refine_coarsen_enum`、`refine_coarsen_time`、`refine_period`、`write_interval_step`、`max_time_step` 这类「不常改但会改网格/输出/AMR 调度」的项。例如 GOLDEN 锚点使用 `refine_period=4`、`refine_coarsen_time=0.0001`，而调试期常被改成 `period=1`、`time=0.0`——二者都会改变首次 AMR 时机和频率，必须对齐到锚点才能回到同样网格。
4. **运行类脚本若未显式写某些字段**（本项目 `run_tests.py` 只写 `which_case/end_time/enable_amr/minus_level/max_level/refine_coarsen_enum/write_interval_time/max_time_step`，**不写 `refine_err/coarsen_error`，也不写 `refine_period/refine_coarsen_time`**），它就会静默继承当前 `param.ini` 或源码里的残留值——这是常见的隐性不一致来源，务必人工补上锚点值（脚本里 `update_param` 加 `'refine_err': 1.0, 'coarsen_error': 0.8, 'refine_period': 4` 这类，并在源码层把 `refine_coarsen_time` 也改为锚点值）。
5. **验证通过后把 param.ini 恢复**，避免把调试期的阈值等残留带回日常状态。

**结论**：回退测试的第一步永远是「**高频调试 IO 检测/关闭** + 配置一致性审计」，否则要么算例慢到让人误判、要么数值比对（尤其 Amr 细胞数）失真。

---

## 五大智能体 SOP

### 1. 裁判验证智能体 (Referee Validation Agent)

**角色**: 严格裁判，只判决，不修复。

**调用时机**: 任何代码修改后，需要官方验证并提交时。

**完整 SOP**:
1. **智能体定义约束**: 必须以 `enable_write_tools=true` 权限定义/唤醒。
2. **首步提权**: 唤醒后的第一步，必须强制使用 `ask_permission` 批量申请底层环境的持久化运行授权 (`make`, `git`, `python`, `$env:PATH`, `.\bin\AMR_Solver.exe`)，避免中途因无终端权限被静默阻塞。
3. 运行：`powershell -File .\validate_current.ps1` 或根据配置文件手动执行所有算例。
4. **若通过（Exit Code 0 / 0误差）**:
   - `git add .`
   - `git commit -m "chore: [GOLDEN-PASS] pass validation regression suite"`
   - `git push origin main`
   - 报告 PASS + commit hash
5. **若失败（Exit Code 1 / 误差超标）**:
   - **严禁修改源码**
   - 立即向用户/调度者报告失败指标
   - 附上失败算例名称和误差数值

**硬约束**: 验证智能体永远不修改 `.cpp` 文件。

---

### 2. MPI 并行调试智能体 (MPI Debug Agent)

**角色**: MPI 并行 BUG 专家，专注通信、幽灵数据、分区边界问题。

**调用时机**: MPI 并行运行崩溃、能量非守恒、结果与串行不一致时。

**完整 SOP**:
1. **建立 BUG 清单（5~8 条）**: 先从代码分析推断潜在问题，列成条目
2. **串行逐条推进**: 每次只修一条，不跳跃
3. **每修一条后必须验证**:
   - 串行 [GOLDEN-PASS]: `powershell -File validate_current.ps1`
   - 确认串行通过后，再测 MPI: `mpiexec -n 4 ./bin/AMR_Solver.exe`
4. **失败即回退**: 若串行测试失败，立即 `git checkout -- src/`
5. **禁止批量修改**: 单步单验证

**调试重点领域（按优先级排列）**:
1. **【最高优先级】p4est_iterate 接口的影像区（ghost）数据交互审计**:
   串并行不一致时，**很大概率是并行通信没做好**。最优先排查**所有**调用 `p4est_iterate` 的接口函数，**尤其是**其回调涉及 `p4est_iter_face_info_t` 或 `p4est_iter_corner_info_t` 的（面/角迭代会跨 rank 访问邻居单元）：
   - 检查 `p4est_iterate` 的 `ghost` / `ghost_data` 入参是否真的传入了**已创建并完成数据交换**的 ghost，而不是 `NULL`；
   - 逐一核对回调内部访问邻居/主点单元时，是否按 `side->is_hanging.is_ghost[]` / `side->is.full.is_ghost` / `is_ghost` 正确走 `ghost_data[...]` 分支，还是错误地直接用本地 `p.user_data`；
   - **一旦发现某个接口漏传 ghost（传入 NULL）或回调内有影像区分支缺失，立即停止后续调试并报告**，先把该通信补齐再做任何逐单元数值比对。
   - 反例佐证（本项目）：step2/3 BUG 根因是 `ComputeHangingNodeVelocity...` 的 `p4est_iterate` 传了 `NULL` ghost；step4 BUG 根因是 `refresh_after_balance` 的 `p4est_iterate` 传了 `NULL` ghost。二者都表现为"跨 rank 的 coarse-fine / 悬挂面被漏算"，串行正常而 MPI 错。
2. Ghost 层数据失效（p4est_refine/coarsen 后未重建 ghost）
3. 迭代回调中 NULL ghost 解引用
4. 分区边界的节点力不对称
5. 全局量 Allreduce（能量、时间步）
6. partition 后的数据迁移（`p4est_partition` 时序）

**全场 VTU 锚点比对策略 (Debug Anchor Strategy)**:
1. **植入锚点**: 在目标代码处（如黎曼求解器后、节点速度组装后），插入 `IOAlgorithm::p4est_debug_output_vtu(p4est, "output/debug", 0, location_id);`，对全场物理量进行快照输出。此操作只读，绝对安全。
2. **生成参照**:
   - **串行**: 运行 `.\bin\AMR_Solver.exe` 产生参照文件 `output/debug_checkpoint_0000_locX_0000.vtu`，并重命名为 `ref.vtu`。
   - **并行**: 运行 `mpiexec -n 4 .\bin\AMR_Solver.exe` 产生多区块文件 `output/debug_checkpoint_0000_locX.pvtu`。
3. **强制对齐与比对**: 
   - 使用升级版的 `compare_vtu.py`：`python compare_vtu.py --target output/debug_checkpoint_...pvtu --ref output/ref.vtu --tol 1e-12`
   - 该脚本将利用数据结构中的 `Global_SFC_ID` 作为空间索引，自动将乱序并行的 `.pvtu` 与串行的 `.vtu` 进行精确的网格对齐。
4. **锚点二分法**: 利用该策略在迭代步内不断前移或后移锚点位置（`location_id`），直到精确锁定引发串并行数据第一次发生误差（>1e-12）的代码行。

**MPI 运行命令**:
```powershell
mpiexec -n 4 ./bin/AMR_Solver.exe  # 4核
mpiexec -n 8 ./bin/AMR_Solver.exe  # 8核
```

---

### 3. 数学物理智能体 (Math Agent)

**角色**: 数学推导与物理方程文档化专家。

**调用时机**: 需要推导方程、解释算法、建立 C++ 变量与论文符号的映射关系时。

**职责**:
1. 基于论文推导 Lagrangian 流体动力学控制方程（质量、动量、能量守恒）
2. 推导角节点速度求解器矩阵方程和 p4est AMR 限制公式
3. 时空离散化方案（1 阶显式格式）
4. 建立 `quad_data_t` / `CCorner_data` 物理量分类表
   - 主要状态变量（必须持久化）
   - 几何变量
   - 求解器临时缓存（可清除）
5. 用 LaTeX 格式撰写数学文档（输出为 `math_formulation.md`）

---

### 4. 架构分析智能体 (Code Analyzer / Architecture Agent)

**角色**: 代码库架构审计与重构规划专家。

**调用时机**: 需要理解代码结构、规划重构、识别耦合与瓶颈时。

**分析范围**:
- `src/main.cpp`（约5400行）、`src/alg.cpp`、`src/variable.h`、`src/defines.h`
- 关键数据结构与全局状态
- 函数调用依赖图与执行流
- 代码缺陷与架构瓶颈
- 重构策略（模块化拆分为 Physics/Mesh/TimeIntegrator/IO/Callbacks）

---

### 5. 项目管理智能体 (Task Agent)

**角色**: 动态任务清单维护和里程碑跟踪。

**调用时机**: 里程碑推进后需要更新任务状态时。

**职责**:
1. 维护 `task.md`（`[ ]`未完成 / `[/]`进行中 / `[x]`已完成）
2. 每个任务必须包含具体验收标准
3. 根据用户反馈动态调整优先级
4. 已完成的 BUG 条目在串行验证通过后从活跃清单移除

---

## 三条核心行为规则

### 规则 A：极速一键验证 (Scripted Referee Validation)
- 验证时**必须**调用 `validate_current.ps1`，严禁分步调用 `make` 和测试程序
- 失败即中止，绝对禁止当场修改源码
- 成功立即 `git add && git commit && git push`

### 规则 B：清单化串行排查与黄金前提 (Checklist & Golden Premise)
- 复杂 BUG 必须先列清单（5~8 条），再逐条推进
- 每修一条必须先通过串行 [GOLDEN-PASS]，才能继续下一条
- 若破坏串行，必须无条件回退

### 规则 C：失败只报不修 (Report-Only on Failures)
- 用户指定的运行/测试失败时，只报告失败事实和崩溃分析
- 绝对禁止擅自修改代码尝试修复
- 排错任务由用户手动分配

---

## 当前项目状态（快照）

### 串行基准 [GOLDEN-PASS]
- ✅ Noh 32x32 均匀串行
- ✅ Sod AMR 串行
- ✅ Sedov AMR 串行

### MPI 并行状态
- ✅ 无 AMR 均匀网格（Noh 32x32）：4核/8核 通过
- ✅ AMR 并行：step2/3（relaxed hanging solver 缺 ghost）与 step4（refresh_after_balance 缺 ghost）通信 BUG 已修复，step 3/4/10/50/54 串并行逐位一致
- 🔄 待处理：step55 首次分歧已定位为 AMR 粗化阈值临界 FP 翻转（详见下文方法学），**尚未回归测试、正式修复未应用**

### 主要代码位置
- 时间推进主循环: `main.cpp: advance_time_step()` (~L5160)
- AMR 回调（refine/coarsen/balance replace）: `main.cpp: Lagrangian_replace_quads()` (~L4130)
- 节点速度求解: `main.cpp: RiemannSolver()` (~L3891)
- 能量守恒检查: `main.cpp: StatTotalEnergyError()` (~L3752)
- Ghost 层刷新: `main.cpp: refresh_after_balance()` (~L4734)

---

## 串并行不一致的系统化定位方法学 (Systematic Serial/MPI Divergence Localization)

> 本节整理自 2026-08 实战（Sod AMR 从 step100 一路二分到 step55，并定位到物理量差来源）。核心思路是**三层递进**：先定步数 → 再定函数 → 最后定物理量差来源。

### 第 0 步：稳定网格对齐 —— 永远不要用漂移后的物理坐标对齐

在拉格朗日框架下，一旦发生发散，网格的物理坐标 `(cx, cy)` 会随错误的速度场漂移。因此一旦网格被污染，**绝不能**依赖物理坐标跨串并行对齐单元。稳定对齐键见下方「全局 ID 的局限性」。

### 第 1 步：时间步二分，缩小首次分歧发生的步数窗口

1. 用自动化的单命令工具（本项目 `quick_consistency_test.py`）在**任意指定步数**上分别跑串行与 MPI，自动比对单元数、稳定网格集合、全部关键字段。
   ```bash
   python quick_consistency_test.py --step N --ranks 2 --tol 1e-10
   ```
2. **判据（硬规则）**：
   - 若串/并**单元数不同** → 立即停止字段比对（拓扑已发散），只记录数量差，向下二分；
   - 若单元数相同 → 再用稳定几何键核对真实单元集合；集合一致才进入字段比对。
3. **二分策略**：先测一个远步（如 step100），若已发散发散，就向下取中点（如 step50），逐次收敛到「上一个一致步」与「第一个分歧步」之间。本项目最终收敛到 step54 全一致、**step55 为首次分歧**。
4. 快速工具自动完成参数备份/恢复、输出隔离、时间戳分目录，避免污染 `output/`。**重要**：串行输出必须先保存再跑 MPI，否则被后一次运行覆盖。

### 第 2 步：步数确定后，用阶段锚点二分，定位首次分歧的**函数**

1. 在 `advance_time_step` 的主流程里，沿时间推进的关键子阶段放置**只读快照**（如 `STEP_BEGIN` / `AFTER_PREPROCESS` / `AFTER_AMR_REFRESH` / `AFTER_RCP` / `AFTER_MATRIX` / `AFTER_CORNER_SOLVE` / `AFTER_HANGING` / `AFTER_ACCEPT`）。
2. 重点在**目标单元**（上一步已用稳定键锁定）上打印要追踪的物理量（角点速度 `cur/lag`、密度、压力等），串/并各跑一遍，逐阶段比对。
3. 找到「前一阶段一致、后一阶段首次不一致」的那个临界点，落点处的函数就是**首次制造串并行状态差异**的函数。本项目 step4 就是靠 `BEFORE_AMR_REFRESH`（一致）→ `AFTER_AMR_REFRESH`（首次不同）锁定 `refresh_after_balance → quadrant_update_after_balance_callback`。
4. **区分两个概念**：
   - **首次产生错误的位置**（真正根因所在函数）；
   - **错误传播后首次被输出/观察到的位置**（可能比产生位置晚很多函数）。本方法始终追踪"首次产生"，而不是"首次看到"。

### 第 3 步：函数确定后，定位首次分歧的**物理量及其来源**

1. 在该函数内，打印它**输入**（来自邻居/主点/ghost 的各分量）与**输出**（写入角点的最终量）。本项目 step4 关键输出：
   ```cpp
   m_child1_vara->VecCnData[idcnVelocity_cur][hanging_corner] = middle_velo;
   ```
2. 逐分量比对串/并，找出**第一个数值出现差异的中间变量**（如 `middle_velo`、`master_velo[]`、`idcnVelocity_lag[2]`）。
3. **因果验证（最小修改）**：只做一处"更正确"的假设性修改（如把 `refresh_after_balance` 的 `p4est_iterate` 从 `NULL` ghost 改为传入 `ghost/ghost_data`），重跑目标步，若串并全场逐位一致（`max_abs=0`），则证明该处就是差异来源。
4. 判定差异来源的类型：
   - **通信缺失**（最常见）：某接口漏传 ghost → 跨 rank 面/角被漏算 → 目标函数物理量错。本项目 step2/3、step4 均属此类。
   - **阈值临界 FP 翻转**（step55 属此类）：物理场逐位一致，但由邻居差商算出的梯度标量在跨 rank 处有 ~1e-4~1e-2 的浮点非确定性，恰逢 AMR 判据阈值（如 `coarsen_error=0.05`）临界，把细/粗决策翻转。这类诊断要点是：**先证明两套物理场本身逐位一致，再证明真正 flip 的是判断依据（梯度），而不是漏算。**

### 快速一致性工具的自动化原则（实操纪律）
- 参数必须在 `finally` 中恢复（`param.ini` 的 `max_time_step` 等）；
- 串行输出保存后才运行 MPI；
- 每次结果用 timestamp 隔离目录；
- 单元数不同立即停止字段比较；
- 网格集合一致后才比较字段；
- 所有校验脚本**动态解析**步数，严禁硬编码目标步数。

### 全局 ID 的局限性与稳定对齐键 (Global ID Caveat & Stable Key)

在 `p4est_iterate` 的回调中，用如下公式试图获取"网格全局编号"：
```cpp
long long global_id = info->p4est->global_first_quadrant[info->p4est->mpirank] + info->quadid + tree->quadrants_offset;
```

> **注意**：`info->quadid` 是 p4est 在当前 rank、当前 tree 上下文下的**局部编号**，**不是跨分区稳定、永久有效的全局编号**。上面这个公式只有在单 tree、且各 rank 起始偏移恰好对齐的简单场景下，数值上才近似等于全局 SFC ordinal。在**多 rank 分区 / AMR 细分之后**，用 `global_id` 去跨串并行对齐单元**不可靠**，容易出现对不齐、甚至错位的假象。
>
> **更稳健的稳定对齐键**：使用单元的
> `(level, 逻辑 x, 逻辑 y)` —— 即 `info->quad->level`、`info->quad->x`、`info->quad->y`。
> 这三个值是 p4est 的客观几何身份，**在任何 rank、任何分片下都逐位一致**，是跨串并行逐单元比对的可靠 Key。
> ```cpp
> // 稳定对齐键 = (level, x, y)，作为 Python 比对脚本的 key
> key = (info->quad->level, info->quad->x, info->quad->y)
> ```
> **推荐做法**：当发现 global_id 难以对齐（或怀疑对齐失真）时，立即改用 `(level, x, y)` 作为串并行逐单元比对的 Key。这不依赖网格全局编号，也不依赖发生漂移后的物理坐标。

### 全场 VTU 锚点比对策略 (Debug Anchor Strategy) [备用]

若无需逐单元文本日志，可用全场 VTU 快照锚点：
1. **植入锚点**: 在目标代码处插入 `IOAlgorithm::p4est_debug_output_vtu(p4est, "output/debug", 0, location_id);`（只读，安全）。
2. **串行参照**: `.\bin\AMR_Solver.exe` 生成 `output/debug_checkpoint_0000_locX_0000.vtu` 并改名 `ref.vtu`。
3. **并行**: `mpiexec -n 4 .\bin\AMR_Solver.exe` 生成多区块 `.pvtu`。
4. **锚点二分**: 用 `compare_vtu.py`（利用 `Global_SFC_ID` 对齐）前后移动 `location_id`，锁定首次误差 >1e-12 的代码行。
   ```powershell
   python compare_vtu.py --target output/debug_checkpoint_...pvtu --ref output/ref.vtu --tol 1e-12
   ```

---

## Critical Principles for Debugging the Main Loop

* **Include AMR Routines in Checkpoints:** AMR adaptation steps (e.g., `p4est_refine`, `p4est_balance`, `refresh_after_balance`, `quadrant_update_after_balance_callback`) are NOT just topological mesh changes. They perform critical physical field interpolations for new/hanging nodes. **They must be treated as physical computations.**
* **No Blind Spots:** When performing step-by-step or sub-step bisection to locate a divergence, **every function inside the main time loop** must be considered a candidate for checkpointing. Never assume a function is "safe" or "unrelated" if it operates on the physics state.

## ⚠️ 调试排错四大避坑指南 (4 Critical Pitfalls to Avoid)

在执行后续的二分法调试或测试时，请务必当心以下四个极易踩中的坑：

1. **代码插入偏差与回退 (Code Replacement Pitfalls)**
   - **坑**：使用文本替换工具注入 `quadrant_debug_dump_callback` 时，如果锚定词（如 `RiemannSolver`）不够唯一，极易匹配到文件上方的废弃代码，导致主干代码被大面积覆盖。
   - **避坑准则**：在做代码替换前，务必先用精确查询定位到准确行号，尽量缩小修改范围。一旦发现改乱了，**立刻执行 `git restore <file>` 或 `git checkout -- <file>`** 进行安全回退，然后再换个精确定位点（例如 `AcceptNumericalSolution`）重试。

2. **环境变量编译问题 (MSYS2 PATH Environment)**
   - **坑**：智能体在全新的上下文或子会话中直接运行 `make`，会报 `CommandNotFoundException`。
   - **避坑准则**：Windows 下必须手动附加 MSYS2 编译环境。执行编译前必须使用组合命令：
     `$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;' + $env:PATH; make clean; make -j8`

3. **参数文件限制 (param.ini Constraints)**
   - **坑**：辛辛苦苦配置好了第 100 步的代码回调，结果程序跑到第 2 步就停了。
   - **避坑准则**：在执行长步数测试前，务必检查 `param.ini` 中的 `max_time_step`。之前的测试很可能把它锁死在了 2 步，必须动态将其放开。

4. **比对脚本写死 (Hardcoded Python Scripts)**
   - **坑**：Python 比对脚本跑完却没输出结果。因为脚本最初是为了测 1、2 步写的，里面写死了 `for step in ['Step 1', 'Step 2']`。
   - **避坑准则**：任何校验脚本必须写成**动态解析**的形式（通过解析文本中的 Step N 动态生成键值），绝对禁止在脚本里硬编码测试的目标步数。
