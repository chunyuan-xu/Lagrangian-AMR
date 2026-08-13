# Lagrangian-AMR 程序重构提纲

> 当前 G0～G3 黄金回退的唯一可执行 SOP 是 [`golden-gates.md`](golden-gates.md)。本文保留重构阶段、回退锚点和历史证据；其中的门禁原则必须与该 SOP 一致，历史 PASS 记录不能替代当前提交的实测 summary。

> 目标：在不破坏现有数值结果和 p4est/MPI 并行能力的前提下，把当前“以 `src/main.cpp` 和大量 `p4est_iterate` callback 为中心”的实现，逐步重构为职责清晰、通信契约显式、数值内核可单测、串并行可回归的模块化架构。
>
> 本文是重构路线提纲，不建议一次性大改。每一阶段都必须在独立提交中完成，并通过串行锚点与 MPI 一致性门禁后再进入下一阶段。

---

## 1. 当前架构概览

### 1.1 现有文件与职责

| 文件/目录 | 当前主要职责 | 主要问题 |
|---|---|---|
| `src/main.cpp` | 程序入口、初始化、主时间循环、AMR、ghost 生命周期、Riemann 求解、悬点处理、状态更新、输出、诊断 | 职责高度集中；51 个 `p4est_iterate` 调用、49 个 `void *` 回调；算法依赖回调注册顺序和隐式通信时序 |
| `src/variable.h` | 通过枚举索引和裸数组保存单元、边、角点、当前/半步/滞后状态 | 字段缺乏类型语义，错误索引可编译通过；状态阶段容易部分更新 |
| `src/defines.h` | 配置、枚举、运行时状态、输出文件流 | 配置、运行状态和 IO 资源混合；大量 `int` 代替强类型枚举；构造函数包含文件副作用 |
| `src/alg.cpp/.h` | 几何算法、物理公式、初始/边界条件 | 几何、物理和算例初始化边界仍可继续拆分 |
| `src/amr/amr_criteria.h` | p4est refine/coarsen 判据 | policy 直接依赖 p4est 与完整单元结构，难以单测；coarsen sibling 逻辑需优先核验 |
| `src/core/vector_matrix.h` | 二维向量和矩阵 | 非标准 `operator--` 表示取负，`operator^` 表示点积；判等容差硬编码为 `1e-100` |
| `src/io/config_parser.*` | 读取 ini 配置 | 已具备基础边界，但配置校验与默认值仍在 `p4est_data_t` 中 |
| `src/io/vtk_writer.h` | VTK 输出声明/薄封装 | 大量实际输出和调试逻辑仍位于 `main.cpp` |
| `src/solver/corner_solver.h` | 角点矩阵和速度求解声明 | 主要实现仍位于 `main.cpp`，模块尚未真正落地 |
| `src/physics/eos.h` | EOS 纯函数 | 方向正确，但被状态头文件不必要地包含 |
| `src/physics/stage_policy.h` | 单阶段推进语义（M1.4） | 由 `advance_single_stage` 调用；时间循环仅迭代一次 |
| `src/physics/timestep_reduction.h` | 时间步缩小策略（M1.2） | 已接管 dt 缩小判定 |
| `src/solver/solver_gate.h` | 求解器门禁开关（M1.3） | 控制数值内核分支 |
| `src/amr/coarsen_family_policy.h` | coarsen 族策略（M1.1） | 统一 sibling 组判定 |
| `src/io/output_stamp.h` | 输出时间戳语义（M1.5a） | 与刷新幂等性（M1.5b）配套 |
| `src/core/simulation_config.h` | 类型化配置（M2.1） | 已含 `valid()` 校验 |
| `python/run_mpi_gates.py` | 4 核 Sod/Sedov 并行黄金门禁（G3） | G2 特定步数（3/4/10/50/54）部分已于 2026-08-04 退役 |
| `python/run_tests.py` / `python/compare_vtu.py` | 三个串行锚点回归及 VTU 比较 | 已固定黄金参数、Python 环境、参数恢复和安全输出轮转 |

### 1.2 当前主调用链

```text
main
└─ 初始化 MPI / sc / p4est / connectivity / 初值
   └─ advance_time_step
      ├─ PreProcess
      │  ├─ Gradient_estimate
      │  ├─ set_default_coarsening_tag
      │  └─ set_default_refining_tag
      ├─ [满足 AMR 周期/时间条件时] p4est_refine_ext
      ├─ [同一条件分支] 重建 ghost + exchange
      ├─ [同一条件分支] set_allowing_coarsening_tag
      ├─ [同一条件分支] p4est_coarsen_ext
      ├─ [同一条件分支] p4est_balance_ext
      ├─ [满足重分区周期/时间条件时] p4est_partition
      ├─ 重建 ghost + exchange
      ├─ refresh_after_balance（M1.5b：字节级幂等，重复调用不改变结果）
      ├─ ghost exchange
      ├─ predict_timestep + MPI_Allreduce(MIN)
      ├─ write_solution（当前位于物理推进之前）
      ├─ advance_single_stage（M1.4：单阶段语义，时间循环仅迭代一次）
      │  ├─ CalculateHalfTimeVariable
      │  ├─ CalculateCornerRcpLcpNcp
      │  ├─ ghost exchange
      │  ├─ Get_AMR_BDY_info
      │  ├─ ghost exchange
      │  └─ RiemannSolver
      │     ├─ MatrixAssemble
      │     ├─ ghost exchange
      │     ├─ ComputeCornerNodeVelocity
      │     ├─ ghost exchange
      │     ├─ ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes
      │     ├─ ghost exchange
      │     └─ ComputeCornerAndEdgeForce
      │  ├─ ComputeDivergence
      │  ├─ ComputeCoordinate
      │  ├─ UpdateDensity
      │  ├─ UpdateMomentumEquation
      │  ├─ ComputeWork
      │  ├─ UpdateEnergyEquation
      │  ├─ UpdateEquationOfState
      │  └─ ComputeSoundSpeed
      ├─ StatTotalEnergyError（AMR 分支内，需与全局误差统计收敛）
      ├─ AcceptNumericalSolution
      └─ 更新时间与步数
```

### 1.3 当前最关键的架构症结

1. **通信时序是隐式算法的一部分。** 多个 callback 是否正确，取决于调用前是否已建立与交换 `ghost/ghost_data`。
2. **ghost 数据的语义不明确。** 部分 callback 不仅读取 ghost mirror，还会写入它；这种修改不会自动回传 owner rank，通常只作为当前 rank 的临时 scratch。
3. **`void *user_data` 缺少类型保护。** 同一参数在不同 callback 中可能代表 `p4est_data_t *`、`quad_data_t *ghost_data` 或 `NULL`。
4. **状态存储缺少类型与阶段边界。** `DouCData[id...]`、`VecCnData[id...][corner]` 允许任意索引组合，当前/half/lag/relaxed 状态容易只更新一部分。
5. **数值内核和 p4est 适配耦合。** AMR policy、角点求解、悬点约束及 transfer 难以脱离 MPI/p4est 做单元测试。
6. **文件拆分已开始但没有完成。** `corner_solver.h`、`vtk_writer.h` 主要只有声明，核心实现仍留在 `main.cpp`。

---

## 2. 重构总原则

### 2.1 必须遵守的顺序

```text
冻结基线
→ 修复/确认 P0 正确性问题
→ 明确状态和通信语义
→ 抽取纯数值内核
→ 拆分 p4est adapter
→ 缩减主程序
→ 再做性能优化
```

不能先机械拆文件再处理通信和状态语义。否则只是把隐式耦合从一个大文件分散到多个文件，调试会更困难。

### 2.2 两条黄金原则

#### 黄金原则一：每个子里程碑都必须通过串行三黄金回退

任何子里程碑只有在以下三个完整串行算例均通过黄金参考比对后，才允许标记完成：

1. Noh Uniform；
2. Sod AMR；
3. Sedov AMR。

统一门禁命令为：

```powershell
powershell -File .\validate_current.ps1
```

三算例必须使用固定黄金配置，并与 `reference/` 中对应参考解比较；不能用短步冒烟、编译成功或单个算例通过代替三黄金回退。并行黄金门禁（G3）、单元测试和静态检查是附加门禁，不能替代串行三黄金回退。

#### 黄金原则二：先兼容迁移，里程碑闭环后再删除瘦身

在一个子里程碑的功能迁移阶段，原则上只做新增、包装、转发和调用点切换，不立即删除旧实现。推荐采用：

```text
旧实现保持可用
→ 新接口/新模块落地
→ 逐调用点切换
→ 三黄金回退通过
→ 必要的 MPI/专项门禁通过
→ 再删除确认无调用的旧代码
→ 再跑一次三黄金回退
→ 子里程碑完成
```

如必须提前删除代码（例如旧符号与新接口无法共存），需在提交说明中记录原因、影响范围和恢复方法，并把删除限制在最小范围内。禁止在功能迁移尚未闭环时，以“顺手清理”为由同时删除大量旧代码。

### 2.3 C++ CFD 性能与安全架构铁律

为了防止在模块化与面向对象重构过程中引入性能灾难与 MPI 通信错误，必须全局遵守以下两条铁律：
1. **最内层循环禁用动态多态（虚函数）**：在底层网格遍历（如 `p4est_iterate` 内部或针对百万网格的面/角循环）中替换策略时，严禁使用虚函数表（vtable）。必须采用静态多态（CRTP 模板模式）、函数对象或 Lambda 注入，以避免毁灭性的寻址开销与缓存未命中。
2. **MPI 交换结构体必须是 POD 类型**：用于挂载到 p4est 树上并参与网络通信的数据结构（如 `quad_data_t` 及其内部状态变量），必须永远保持为 **POD (Plain Old Data)**。严禁向其中添加构造函数、析构函数、智能指针或虚指针（vptr），否则会导致二进制序列化和反序列化崩溃。所有业务类（Controller/Policy）仅持有其数据结构指针或引用。

### 2.4 每个子里程碑的统一执行模板

每个子里程碑按以下七步执行：

1. **范围冻结**：写清目标、非目标、涉及文件和预期不变的数值行为；
2. **前置基线**：在修改前运行三黄金回退，确认起点可信；
3. **兼容实现**：新增接口或模块，保留旧实现，避免混入无关重命名和优化；
4. **调用迁移**：按小批次切换调用点，每批至少编译并运行相关短程测试；
5. **主验收**：运行完整 Noh/Sod AMR/Sedov AMR 三黄金回退；
6. **清理瘦身**：仅删除已证明无调用、行为已被新实现覆盖的旧代码，再次运行三黄金回退；
7. **独立提交**：记录变更范围、三算例结果、专项测试、删除清单和已知风险。

若第 5 或第 6 步失败，子里程碑不得完成；优先恢复到上一个通过三黄金回退的提交，再定位问题。

### 2.5 M3.4～M3.5 细粒度闭环工作流（经验固化）

M3.4～M3.5 的实践表明，通信语义重构不应按“大功能完成后再统一验收”，而应拆成可回退、可判定、可提交的小锚点。推荐采用以下工作流：

1. **建立可靠入口锚点**：先确认当前提交、编译器、MPI launcher/runtime、PATH/DLL、参数文件 checksum 和输出目录状态；入口锚点必须通过完整 G0～G3，不能只依赖历史记录。
2. **单一假设、单一改动**：每个子锚点只处理一个 callback、一个字段组、一个访问权限或一个生命周期边界；明确修改范围和不触碰的相邻路径，禁止把未解释的实验改动继续叠加。
3. **先读审计，后写代码**：记录 callback 的 `Requires/Reads/Writes/Invalidates/Exchange`，区分 owner-local、remote snapshot 和 scratch；未证明为死路径的 callback、上下文和兼容入口暂不删除。
4. **小步验证**：先完成 G0 和专项测试，再执行完整串行 G1；只有 G1 通过才允许把该小锚点视为数值行为未回归。
5. **完整并行收口**：每一个达到收口条件的粒度——包括 callback 子锚点、字段组子锚点、清理子锚点和文档/协议子里程碑——都必须执行完整四进程 G3：Sod AMR 和 Sedov AMR 两个算例均实际运行并通过。短 MPI smoke、单 rank、单算例或 runner 因首个失败而跳过的算例，都不能替代 G3。
6. **失败即停在当前粒度**：任一 G0～G3 失败，不进入下一子锚点，不提交为完成状态；保留失败证据，定位“最近 PASS 锚点 → 当前 FAIL 改动”的最小区间，必要时二分回退。诊断版、非 canonical launcher、断言路径或无输出 segfault 只能作为环境/构建证据，不能冒充 G3 根因。
7. **通过即提交并追踪**：G0～G3 全部通过后，检查 staged diff 只包含当前粒度的源码/文档，排除参数、黄金参考、可执行文件、日志和输出目录；随后创建 focused commit，并按项目授权推送到 GitHub，使每个通过锚点都可追踪。提交信息必须记录门禁摘要、参数恢复和已知风险。
8. **提交后再确认状态**：记录 commit hash、工作树状态和下一锚点；若推送或提交前发现门禁证据缺失、参数未恢复或工作树混入未知修改，则停留在当前锚点，不把未知中间态升级为基线。

该流程的核心不是增加测试数量，而是缩小“一个改动对应一个结论”的范围：任何失败都能归因到当前最小粒度，任何通过都能形成可恢复的 G0～G3 追踪点。M3.4 的 owner-write 审计和 B1～B15 子锚点验证采用了这一模式；M3.5 的 G3 漂移则证明，在完整 G3 未闭合前不应启动旧路径删除或清理阶段。

### 2.6 会话上下文持久化规则（resume 保护）

每个子里程碑完成（G0～G3 通过并提交）后，必须同步更新 `docs/context.md`，把本次对话的核心内容固化下来，防止后续会话因 resume 失败丢失上下文。更新内容包括：

1. **进度表**：M9（或当前里程碑）各阶段的状态与收口提交 hash；
2. **门禁记录索引**：新完成子任务对应的 `docs/golden-gates-*.md` 文件名；
3. **剩余任务**：当前里程碑未完成项的准确清单与归属模块（含 main.cpp 残留壳函数行号）；
4. **环境事实**：本次实测且复现成本高的环境细节（构建命令、PATH、param.ini checksum、门禁耗时等），便于新会话直接复用；
5. **下一步建议**：明确的后续动作与调用点迁移注意点。

规则要点：

- `context.md` 是**摘要文档**，不是日志：只记录“能帮助新会话从上次收口点续跑”的信息，不记录失败排查全过程；
- 每次更新随该子里程碑的 focused commit 一并提交，或作为独立 docs commit；
- 若多个子里程碑在同一会话连续完成，允许合并为一次更新，但每个收口点都应在该次更新中可追踪；
- 门禁失败或未收口的中间态**不写入** `context.md`，避免把未验证状态固化。

### 2.7 大版本收口后的诊断产物清理规则

每个大版本（如 M9、M10）全部子里程碑收口并提交后，必须清理仓库根目录下未被 git 管理的诊断/调试产物，防止工作区被历史调试文件污染。清理范围：

1. **运行日志**：根目录 `*.log`（如 `anchor_regression_current.log`、`diagnostic_energy_trace_*.log`、`m0_1_g1.log`）；
2. **诊断输出文件**：`*.plt`（`EnergyError.plt`/`DistanceProfiles.plt`）、`ErrorFile.txt`；
3. **机器可读摘要**：`serial_golden_summary.json`、`mpi_gate_summary.json`（运行即重新生成）；
4. **临时调试源码**：`print_size.cpp` 等一次性诊断程序；
5. **Python 字节码缓存**：未跟踪的 `__pycache__/`；
6. **历史取证目录**：`.historical-g3-*`、`.tmp/`、`build_*.o` 等孤儿对象文件；
7. **退役门禁产物**：未跟踪的 `step_tests/`（G2 已 retired 的逐步测试目录）；
8. **其他未跟踪诊断**：`reference/` 下的 `.log`（保留 `.vtu`/`.pvtu` 黄金参考）。

规则要点：

- **只删未被 git 管理的文件**：先用 `git ls-files --others --exclude-standard` 确认未跟踪，**绝不删除任何已跟踪文件**（`D` 状态表示误删，需 `git checkout --` 恢复）；
- **已跟踪但属产物/误提交的文件**（如历史误提交的 `__pycache__/*.pyc`）不走本规则，如需清理走正式提交删除，不纳入版本收口自动清理；
- **保留业务文档与第三方目录**：`prompt/`（论文文稿等非诊断内容）、`libsc-2.8.5/`、`p4est-2.8.5/`、`third_party/`、`.claude/`/`.gemini/`/`.workbuddy/` 等不删；
- **清理随收口 docs commit 一并提交**，或作为独立清理提交，使每次大版本收口后工作区回到干净状态；
- **禁止删除黄金参考**：`reference/*.vtu`/`*.pvtu` 是 G3 比对基准，永不清除；
- 若清理后需要复现历史诊断，git 历史与门禁记录文档（`docs/golden-gates-*.md`）仍是权威证据，重建快照目录无需保留。

### 2.5 每次提交的边界

每个重构提交应满足：

- 只处理一个概念，如“封装 ghost 生命周期”或“抽取 AMR criterion”；
- 不同时进行算法修复和大规模重命名；
- 子里程碑收口提交必须附 G0、G1、G3 结果；G2 仅记录为 retired/N/A，不再作为当前门禁；
- 先保证串行三黄金锚点，再执行该子里程碑要求的 MPI/专项门禁；
- 比较单元时使用 `(treeid, level, x, y)`，不依赖 `quadid`；
- 若网格数量或稳定几何集合不同，立即停止字段比较并定位拓扑首分歧；
- 任何 `p4est_iterate` face/corner callback 都必须显式说明是否需要 ghost、读哪些字段、写哪些字段、何时 exchange。

---

## 3. P0：重构前的正确性冻结与风险修复

> 本文第 3～12 节中的 P0～P4 表示架构问题的优先级分组，不是可直接验收的实施里程碑；真正的执行顺序、子里程碑和门禁以第 13 节 M0～M7 为准。
>
> 优先级最高。P0 未完成前，不进行大规模类设计或目录迁移。

### 3.1 建立不可变回归基线

#### 工作项

1. 固定三个串行锚点：
   - Noh Uniform；
   - Sod AMR；
   - Sedov AMR。
2. 通过 Git 核对锚点版本的全部配置：
   - `param.ini` 参数；
   - `src/defines.h` 默认值；
   - 源码硬编码和宏；
   - 边界条件、迭代次数和输出时机。
3. 固化现有 `python/run_tests.py` 与 `python/_anchor_regression.py`：
   - 保持每个算例显式设置完整黄金配置；
   - 保持 `finally` 恢复 `param.ini`；
   - 保持使用同一 Python 解释器调用比较器；
   - 保持安全轮转输出目录且只输出末帧；
   - 补充统一的机器可读摘要和退出码约定。
4. 固定并行黄金门禁（G3）：
   - 4 核 Sod AMR 与 Sedov AMR，与 `reference/par4_*` 逐点比对（容差 1e-12）；
   - 由 `python/run_mpi_gates.py` 单命令执行；
   - 原 G2 特定步数（3/4/10/50/54）与 step-55 追踪已于 2026-08-04 退役。
5. 保存关键守恒量与字段 checksum：质量、动量、总能量、网格数量、稳定网格键集合。

#### 验收标准

- 单条命令完成全部串行锚点和并行黄金门禁；
- 测试结束后配置必定恢复；
- 测试结果不依赖调用者当前 `param.ini` 的残留值；
- 失败能区分：运行失败、拓扑不一致、字段不一致、环境缺失。

### 3.2 核验并处理 P0 数值风险

> **P0 状态（2026-08-04）**：本节 A～F 六项已全部由 M1 系列子里程碑闭环，并保留对应提交与 G1/G3 证据。条目保留为设计依据与风险档案；后续如需复现或再评估，直接查阅对应 M1.x 记录（A→M1.1、B→M1.2、C→M1.3、D→M1.4、E→M1.5a、F→M1.5b）。

以下事项已从源码中观察到，必须先通过最小测试确认设计意图，再单独修复：

#### A. 时间步局部最小值

`quadrant_predict_timestep_callback` 当前直接覆盖：

```cpp
p4est_data->local_dt = min(quad_cfl_dt, min(quad_vol_dt, quad_increased_dt));
```

这会只保留本 rank 最后遍历单元的 `dt`，随后 `MPI_MIN` 也只是比较各 rank 的最后一个局部值。应改为“rank 内累计最小值 → rank 间最小值”，并测试结果与遍历顺序无关。

#### B. coarsen sibling 判据

`CoarsenErrorEstimate` 当前在任一 child 低于阈值时立即返回允许粗化；Distance 分支也会在循环第一个 child 处返回。需要明确设计究竟是：

- 所有 children 都满足才允许粗化；
- family 最大梯度低于阈值；
- family 平均梯度低于阈值。

确定语义后建立 sibling 顺序置换测试，再修改实现。

#### C. solver 枚举字段疑似误用

`two_stage_Runge_Kutta` 中当前比较：

```cpp
p4est_data->coord_type == p4est_data_t::RiemannSolver::GridAligned
```

这是跨枚举类型的比较，当前 `plane` 与 `GridAligned` 恰好都取整数 0。必须先核对设计意图：条件可能应检查 `coord_type == plane`，也可能应检查 `solver_type == GridAligned`；在确认前不能直接替换，但必须消除对枚举整数碰巧相同的依赖。当前实现还可能使非平面坐标分支完全跳过 Riemann 求解，因此应提升为 P0。

#### D. RK 阶段语义

函数名为 `two_stage_Runge_Kutta`，但循环条件是 `iter_num < 1`，`case 1` 不可达。需决定：

- 当前算法本就是单阶段：重命名并删除不可达分支；
- 本应为 RK2：补齐第二阶段，并建立解析解与守恒回归。

#### E. 输出时间语义

当前 `write_solution` 在物理推进与 `AcceptNumericalSolution` 之前调用。需统一：

- 文件编号代表“进入第 N 步前”还是“完成第 N 步后”；
- `current_step`、`current_time`、VTK `TimeValue` 与文件名保持一致。

#### F. `refresh_after_balance` 的真实语义与幂等性

该函数虽然命名为“balance 后刷新”，但目前在每个时间步都会执行，即使本步没有发生 balance。它会修正悬点几何、速度及部分热力状态。必须先验证：

- 在没有拓扑变化时重复调用是否幂等；
- current/lag/centroid/EOS 等相关字段是否被完整同步；
- 它应重命名为通用一致性阶段，还是只应在拓扑变化后调用。

### 3.3 清理临时调试代码但保留可控诊断

- 将 step 2/3、目标坐标等硬编码 trace 从主流程迁移到 `Diagnostics`；
- 通过配置或编译选项启用，不在生产路径散布 `if (current_step == ...)`；
- 诊断单元键统一为 `(treeid, level, x, y)`；
- 日志按 rank 和阶段命名，避免并发覆盖。

---

## 4. P1：建立清晰的数据模型和配置边界

### 4.1 拆分 `p4est_data_t`

建议拆为：

```cpp
struct SimulationConfig;   // 只读配置
struct SimulationClock;    // current_time/current_step/dt
struct RuntimeMetrics;     // 能量、误差、统计量
struct OutputConfig;       // 输出频率和路径
struct MeshConfig;         // level、AMR、partition 参数
struct SolverConfig;       // CFL、scheme、Riemann、精度
```

#### 要求

- 使用 `enum class` 替代裸 `int`；
- 配置解析后立即验证范围及组合合法性；
- 配置对象构造不得打开文件；
- `EnergyFile`、`DistanceFile`、`ErrorFile` 移入独立 IO/diagnostics 对象；
- 模拟过程中配置只读，运行状态单独可写。

### 4.2 用强类型状态替代枚举索引裸数组

当前 `CVariable` 同时保存 cell/edge/corner 和 cur/half/lag/relaxed 多阶段变量。建议分步迁移：

```cpp
struct ThermodynamicState {
    double density;
    double pressure;
    double internal_energy;
    double total_energy;
    double sound_speed;
};

struct CellGeometry {
    std::array<Vec2, 4> corners;
    Vec2 centroid;
    double volume;
};

struct CellState {
    double mass;
    ThermodynamicState thermo;
    Vec2 centroid_velocity;
    CellGeometry geometry;
};

struct TimeLevelState {
    CellState current;
    CellState half;
    CellState candidate;
};
```

边和角点状态另设结构，不继续混入同一大数组。

#### 迁移策略

1. 先提供 typed accessor 包装旧数组，不立即改变内存布局；
2. callback 改用 accessor；
3. 添加状态不变量检查；
4. 最后再替换底层存储，降低一次性风险。

### 4.3 建立状态不变量

在初始化、refine/coarsen transfer、balance refresh、物理推进和接受解之后检查：

```text
volume > 0
mass > 0
density ≈ mass / volume
pressure ≈ EOS(gamma, density, internal_energy)
sound_speed ≈ c(gamma, pressure, density)
centroid ≈ geometry centroid
total_energy ≈ internal_energy + kinetic_energy
```

AMR transfer 额外检查：

```text
parent mass ≈ Σ child mass
parent momentum ≈ Σ child momentum
parent total energy ≈ Σ child total energy
parent volume ≈ Σ child volume
```

---

## 5. P1：封装 ghost 生命周期与通信阶段

> 这是本项目 MPI 正确性重构的核心，优先级高于普通文件拆分。

### 5.1 引入 `GhostSession`

建议接口：

```cpp
class GhostSession {
public:
    static GhostSession build(p4est_t& forest, p4est_connect_type_t connectivity);
    void exchange();
    const quad_data_t& remote(p4est_locidx_t ghost_id) const;
    void invalidate_after_topology_change();
};
```

#### 强制规则

- `refine/coarsen/balance/partition` 后旧 ghost 立即失效；
- topology 改变后不能继续访问旧 `ghost_data`；
- remote ghost 默认只读；
- exchange 必须由高层 phase 显式触发；
- 不允许业务 callback 自行管理 `P4EST_ALLOC/P4EST_FREE`。

### 5.2 禁止把 ghost mirror 当成权威状态写入

当前多个 face/corner callback 会通过可写 `quad_data_t *` 修改 ghost mirror。该写入不会自动传播到 owner rank。应区分：

```cpp
LocalCellState&             // 权威、可写
const RemoteCellSnapshot&   // 只读
FaceScratch&                // 本 rank 临时结果
CornerScratch&              // 本 rank 临时结果
```

逐步审计这些 callback：

- `quadrant_edge_minmod_estimate_callback`；
- `quadrant_corner_minmod_estimate_callback`；
- `quadrant_whether_allowing_coarsening_from_edge_callback`；
- `quadrant_whether_allowing_coarsening_from_corner_callback`；
- `quadrant_update_after_balance_callback`；
- `quadrant_get_children_hanging_info_callback`；
- `quadrant_set_init_parent_edge_callback`；
- `quadrant_corner_velocity_callback`。

目标模式：

```text
读取 local + remote snapshot
→ 计算共享 face/corner 结果
→ 写入 scratch
→ owner-local commit pass
→ exchange 发布权威结果
```

> **M1.5b 实证**（提交 `a8f91f3`）：`refresh_after_balance` 的重复调用已被验证为字节级幂等——重复执行不改变任何输出字节。这证明 ghost 写入是确定性的本 rank scratch，而非跨 rank 损坏；但按本小节目标，它仍是设计债务，后续应迁移为显式只读 snapshot。

### 5.3 为每个计算阶段定义通信契约

每个 phase 应声明：

```text
Requires: ghost topology generation N, exchanged fields {...}
Reads: local {...}, remote {...}
Writes: local {...}, scratch {...}
Invalidates: {...}
Publishes after exchange: {...}
```

例如：

```text
CornerMatrixPhase
Requires: geometry/half-state 已同步
Writes: local corner matrix contribution
Exchange
CornerReductionPhase
Reads: local + remote contribution
Writes: owner-local corner system
```

### 5.4 用有类型的 callback context 替代 `void *`

示例：

```cpp
struct GradientCallbackContext {
    const SimulationConfig& config;
    const GhostView& ghosts;
    GradientScratch& scratch;
};

struct HangingCallbackContext {
    const GhostView& ghosts;
    HangingScratch& scratch;
};
```

禁止同一个 `user_data` 被分别转换成无关类型。p4est 的 C callback 入口可以保留 `void *`，但只允许在一处 adapter 中进行单一、可检查的转换。

---

## 6. P2：抽取 AMR 子系统

### 6.1 目标结构

```text
src/amr/
├─ amr_controller.h/.cpp      # refine/coarsen/balance/partition 流程
├─ amr_policy.h/.cpp          # 纯 refine/coarsen 判据
├─ amr_transfer.h/.cpp        # parent-child prolongation/restriction
├─ hanging_repair.h/.cpp      # coarse-fine 几何和状态一致性
└─ p4est_amr_adapter.h/.cpp   # p4est callback 适配
```

### 6.2 抽取纯 AMR policy

纯 policy 不应依赖 `p4est_t`、`p4est_quadrant_t` 或 `void *`：

```cpp
RefineDecision evaluate_refine(const CellIndicator&, int level, const AmrConfig&);
CoarsenDecision evaluate_coarsen(const std::array<CellIndicator, 4>&, int level, const AmrConfig&);
```

#### 单元测试

- sibling 顺序变化不改变结果；
- minimum/maximum level 优先级明确；
- 阈值相等时行为明确；
- 压力梯度、密度梯度、距离准则分别覆盖；
- NaN/Inf 和非法配置被拒绝；

### 6.3 抽取 AMR transfer

```cpp
std::array<CellState, 4> prolongate_parent(const CellState&, const ParentGeometry&);
CellState restrict_children(const std::array<CellState, 4>&);
```

p4est callback 只负责：

```text
incoming/outgoing quadrants
→ typed view
→ 调用纯 transfer
→ 写回结果
```

### 6.4 合并 AMR 后处理

当前：

- `refresh_after_balance` 每步都会执行，函数名与真实语义不符；
- `postprocess_after_coarsening` 只有定义，未发现调用；
- `quadrant_update_after_balance_callback` 与 `quadrant_update_after_coarsening_callback` 职责相似。

建议抽取：

```cpp
HangingNodeRepair compute_hanging_repair(
    const MasterEdgeState&,
    const ChildEdgeState&,
    const ChildEdgeState&);
```

然后由统一 `enforce_hanging_consistency` phase 处理。合并前必须比较两套 callback 的逐字段行为，不能直接删除未调用代码。

---

## 7. P2：抽取 Hydro/Riemann 子系统

### 7.1 目标结构

```text
src/hydro/
├─ hydro_step.h/.cpp
├─ predictor.h/.cpp
├─ corner_matrix.h/.cpp
├─ corner_velocity_solver.h/.cpp
├─ hanging_constraint_solver.h/.cpp
├─ force_assembly.h/.cpp
├─ conservative_update.h/.cpp
└─ state_acceptance.h/.cpp
```

### 7.2 显式 phase 化 Riemann 求解

将当前隐式调用链：

```text
MatrixAssemble
→ exchange
→ ComputeCornerNodeVelocity
→ exchange
→ hanging constrained solver
→ exchange
→ force
```

改为：

```cpp
riemann.reset_fluxes();
riemann.assemble_local_corner_contributions();
ghosts.exchange();
riemann.reduce_and_solve_master_corners();
ghosts.exchange();
riemann.solve_hanging_constraints();
ghosts.exchange();
riemann.assemble_forces();
```

函数名应直接反映同步边界和输入输出。

### 7.3 把纯数学计算与 p4est 遍历分开

例如角点速度求解应拆成：

```cpp
CornerSystem assemble_corner_system(span<const CornerContribution>);
CornerSolution solve_corner_system(const CornerSystem&, const BoundaryCondition&);
```

p4est callback 只收集邻接单元数据，不直接包含完整物理算法。

### 7.4 明确共享角点的 owner 语义

当前可能由多个 rank 对共享角点重复计算，再只发布各自本地单元槽位。重构时需决定：

1. **确定性 owner 求解**：一个 rank 负责共享角点，结果广播；或
2. **重复确定性求解**：所有相关 rank 使用固定排序的相同输入，保证结果一致。

无论选哪一种，都必须用 `(treeid, level, x, y, corner)` 形成稳定排序，避免累加顺序随分区变化。

---

## 8. P2：拆分 Mesh/p4est 适配层

### 8.1 目标结构

```text
src/mesh/
├─ forest.h/.cpp
├─ ghost_session.h/.cpp
├─ neighborhood_view.h/.cpp
├─ cell_key.h
├─ p4est_callbacks.h/.cpp
└─ p4est_runtime.h/.cpp
```

### 8.2 隔离 p4est 类型

纯物理、几何、AMR policy 和 transfer 不应直接看到：

- `p4est_t`；
- `p4est_quadrant_t`；
- `p4est_iter_*_info_t`；
- `sc_array_t`；
- `void *`。

只在 `mesh/p4est_*` adapter 层转换。

### 8.3 统一稳定单元键

```cpp
struct CellKey {
    p4est_topidx_t tree_id;
    int level;
    p4est_qcoord_t x;
    p4est_qcoord_t y;
};
```

用途：

- 串并行逐单元比较；
- 诊断日志；
- callback scratch；
- 确定性排序；
- AMR transfer 追踪。

`quadid` 只允许作为当前 rank、当前 forest generation 内的临时索引。

---

## 9. P3：拆分 Geometry、Physics 和基础数学

### 9.1 基础数学类型

将：

- `CDoubleVector` → `Vec2`；
- `CDoubleMatrix` → `Mat2`；
- `operator--` → 标准一元 `operator-`；
- `operator^` → `dot(a, b)`；
- 硬编码 `1e-100` 判等 → 场景化 `nearly_equal(abs_tol, rel_tol)`；
- 矩阵逆 → 带奇异性检查的接口。

先添加兼容层和单元测试，再逐步替换调用点。

### 9.2 拆分 `alg.cpp/.h`

建议：

```text
src/geometry/
├─ polygon.h/.cpp
├─ metric.h/.cpp
└─ coordinate_system.h/.cpp

src/physics/
├─ eos.h
├─ timestep.h/.cpp
├─ divergence.h/.cpp
└─ initial_conditions.h/.cpp
```

`InitialCondition` 和 `BoundaryCondition` 不应继续与通用几何/物理公式混在同一接口中。

---

## 10. P3：拆分 IO、诊断和测试基础设施

### 10.1 IO 模块

```text
src/io/
├─ config_parser.*
├─ simulation_config_loader.*
├─ vtk_writer.*
├─ pvtu_writer.*
└─ profile_writer.*
```

### 10.2 Diagnostics 模块

```text
src/diagnostics/
├─ conservation_monitor.*
├─ state_invariant_checker.*
├─ field_checksum.*
└─ targeted_trace.*
```

诊断模块不应修改数值状态，也不应改变 callback 的通信时序。

### 10.3 测试层次

1. **纯函数单元测试**：EOS、几何、矩阵、AMR policy、transfer；
2. **单 rank 集成测试**：一个时间步、一次 refine/coarsen、悬点约束；
3. **MPI 集成测试**：同一网格在 1/2/4 ranks 下状态一致；
4. **锚点回归**：Noh/Sod/Sedov；
5. **并行黄金回归**：4 核 Sod/Sedov 并行黄金（G3）与守恒量。

---

## 11. P3：统一构建系统与工程规范

当前 Makefile 使用 C++14，并编译 `config_parser.cpp`；CMake 使用 C++11，且 `AMR_Solver` 源文件列表没有包含 `config_parser.cpp`——也未包含 M1.x/M2.x 新增的 header-only 源（`solver_gate.h`、`stage_policy.h` 等）。因此当前 CMake 并不是一个可用构建入口。建议：

1. 选定 CMake 为唯一正式构建入口，Makefile 作为兼容包装；
2. 统一 C++ 标准（建议至少 C++17，具体取决于编译环境）；
3. CMake 纳入所有正式源文件；
4. 明确 third-party p4est 的唯一来源和版本；
5. 输出放在构建目录，不污染源目录；
6. 增加编译告警与可选 sanitizer；
7. 将测试接入 CTest 或统一 Python runner。

---

## 12. P4：最终主程序目标

最终 `main.cpp` 只保留：

```cpp
int main(int argc, char** argv) {
    RuntimeGuard runtime(argc, argv);
    const auto config = load_and_validate_config("param.ini");
    Simulation simulation(config, runtime.communicator());
    simulation.initialize();
    simulation.run();
    return 0;
}
```

`Simulation::run()` 只负责高层阶段编排：

```text
AMR controller
→ ghost synchronization
→ timestep controller
→ hydro step
→ state acceptance
→ diagnostics
→ output scheduler
```

它不应包含具体 face/corner 数学、不应转换 `void *`、不应直接分配 ghost buffer，也不应包含特定 step/坐标的调试分支。

---

## 13. 可执行里程碑与子里程碑

### 13.1 门禁等级

为避免“完成”的含义随阶段变化，统一使用 G0～G3 门禁；G2 已退休，不再作为有效验收门槛：

| 门禁 | 内容 | 使用时机 |
|---|---|---|
| G0 | 干净编译、静态检查、相关单元测试 | 每个小批次调用迁移后，以及每个收口点 |
| G1 | 完整串行 Noh、Sod AMR、Sedov AMR 黄金回退 | **每个子里程碑前后，以及清理瘦身后；强制** |
| G2 | retired / N/A；不恢复旧的特定 step 检查 | 仅作历史归档，不参与当前验收 |
| G3 | 完整 4 核 Sod AMR 和 Sedov AMR 并行黄金、守恒量和负载合理性 | **每个达到收口条件的子锚点、子里程碑和清理阶段；强制** |

> G3 必须实际执行 Sod 与 Sedov 两个四进程算例；不能用串行成功、单 rank、短 MPI smoke、单个 MPI 算例，或 runner 在首个失败后跳过的算例替代完整 G3。只有 G0～G3 全部通过，当前粒度才允许提交为已完成并推送到 GitHub。

> **G2 退役说明（2026-08-04）**：原 G2（MPI 指定步数 step 3/4/10/50/54 逐点一致性）与 step-55 临界阈值追踪已退役，原因是指定步数验证耗时且与 G3 覆盖重叠。并行正确性统一由 G3（`python/run_mpi_gates.py`，4 核 Sod/Sedov 与 `reference/par4_*` 逐点比对）承担。本文件历史完成记录中出现的 G2 引用保留为归档事实，不再构成当前验收要求。

G1 是串行数值回归门槛，G3 是并行收口门槛；二者均不可由另一个门禁替代。任何一个门禁失败，当前粒度保持阻塞，不得创建“已完成”提交。

### M0：冻结可信基线与执行协议

**目标**：让后续每次重构都有可重复、不会污染配置的统一裁判。

#### M0.1 固化串行三黄金门禁

- **实施**：统一 `validate_current.ps1`、`python/run_tests.py`、`python/_anchor_regression.py` 的职责和退出码；明确黄金配置与参考文件映射。
- **产物**：单条命令、三算例摘要、失败分类、配置恢复证明。
- **验收**：连续运行两次 G1，结果相同，`param.ini` 与运行前逐字节一致。

#### M0.2 固化并行黄金门禁

- **实施**：固定 4 核 Sod/Sedov 为 G3，由 `python/run_mpi_gates.py` 单命令执行并与 `reference/par4_*` 逐点比对。
- **产物**：机器可读 summary，明确 PASS、预期差异和真实失败。
- **验收**：G1、G3 均可由文档命令重复执行。
- **[2026-08-04 修订]**：G2（指定步数 step 3/4/10/50/54）与 step-55 追踪已退役，并行正确性统一由 G3 承担。

#### M0.3 建立子里程碑记录模板

- **实施**：每个子里程碑记录范围、前置 G1、迁移批次、专项测试、后置 G1、删除清单、清理后 G1、提交号。
- **产物**：统一验收记录格式。
- **验收**：后续任务不能在缺少这些证据时标记完成。

**M0 完成条件**：M0.1～M0.3 全部完成，并通过 G1+G3。

**完成记录（2026-08-03）**：M0.1=`69a7522`，M0.2=`2699f2c`，M0.3=`df73722`；收口 G1 的 Noh/Sod AMR/Sedov AMR 全部 PASS 且 `param.ini` 恢复，G2 step 3/4/10/50/54 全部逐位 PASS，G3 四核 Sod/Sedov 与并行黄金参考 PASS。M0 状态：**完成**。

### 13.2 子里程碑验收记录模板

每个子里程碑在提交前复制并填写以下记录；缺少 G1 结果或参数恢复证明时不得标记完成：

```markdown
## <子里程碑编号与名称>

- 目标：
- 非目标：
- 起点提交：
- 涉及文件：
- 预期数值行为：保持不变 / 经批准建立新基线

### 前置门禁
- G1 Noh Uniform：PASS/FAIL，耗时，目标文件
- G1 Sod AMR：PASS/FAIL，耗时，目标文件
- G1 Sedov AMR：PASS/FAIL，耗时，目标文件
- param.ini 恢复：true/false

### 兼容迁移
- 新增接口/模块：
- 已切换调用点：
- 暂时保留的旧入口：
- G0/专项测试：

### 迁移完成门禁
- G1：PASS/FAIL（附 `serial_golden_summary.json` 摘要）
- G3：PASS/FAIL/不适用（列出算例）

### 清理瘦身
- 删除符号/文件清单：
- 无调用证明：
- 清理后 G1：PASS/FAIL
- 清理后 G3：PASS/FAIL/不适用

### 结论
- 状态：完成/阻塞
- 完成提交：
- 已知风险：
- 下一子里程碑：
```

失败排查记录使用稳定区间：`上一个 PASS 提交 → 当前 FAIL 工作树/提交`。若 15 分钟内不能定位，应按调用迁移批次或提交序列把区间二分，每个子区间运行最小可判定门禁；恢复到最近 PASS 点后再继续，不得带着未知失败跨入下一子里程碑。

### M1：P0 数值语义逐项闭环

**目标**：在架构移动前消除会让回归结论失真的高风险语义问题。每项独立完成，禁止合并修复。

#### M1.1 时间步局部最小值

- **实施**：先增加遍历顺序测试和 rank 内统计诊断；确认问题后改为 rank 内累计最小值，再执行 `MPI_Allreduce(MIN)`。
- **专项验收**：改变本地遍历顺序和 MPI rank 数不改变全局 `dt`；通过 G1+G2。
- **清理**：回退通过后再移除临时诊断和旧覆盖路径，随后再次 G1。

**完成记录（2026-08-03）**：起点提交 `6171376`。新增 `TimestepReduction::initial_local_minimum` 与 `accumulate_local_minimum`，在每次 `p4est_iterate` 前以正无穷初始化，并在 volume callback 中累计 rank 内最小值；原有 `MPI_Allreduce(MIN)` 保持不变。遍历顺序专项单元测试 PASS；G2 step 3/4/10/50/54 在 2 ranks 下全部 PASS；清理后 G1 的 Noh Uniform（4112）、Sod AMR（3046）、Sedov AMR（3933）全部 PASS，且 `param.ini` 逐字节恢复。未引入临时诊断，无额外旧路径可删除。状态：**完成**。

#### M1.2 coarsen family 判据

- **实施**：先把现有行为包装为兼容函数；用四子单元顺序置换测试确认目标语义，再切换实现。
- **专项验收**：24 种 sibling 排序结果一致；阈值相等、NaN/Inf、level 边界有明确测试；通过 G1+G2。
- **清理**：新 policy 覆盖全部调用后再删除旧分支，再次 G1。

**完成记录（2026-08-03）**：起点提交 `e4ff042`。将四个 child 投影为脱离 p4est 的 `ChildIndicator` family，由纯函数统一处理禁止标志、level 边界和指标。目标语义冻结为与黄金路径兼容且顺序无关的“family 全局否决后，任一 child 满足指标即可粗化”；梯度使用严格 `< coarsen_error`，Distance 使用严格 `> coarsen_error`，相等和非有限指标不满足条件。24 种 sibling 排列、禁止标志、阈值相等、NaN/Inf、minimum/maximum level 专项测试 PASS；G2 step 3/4/10/50/54 全部 PASS；迁移完成 G1 全部 PASS。曾验证“所有 child 均满足”方案会使 Sod 末帧从 4390 增至 5104 cells，故未更新黄金参考并在本子里程碑内回退为兼容语义。删除 `main.cpp` 中已禁用的重复 coarsen 判据后，清理后 G1 的 Noh Uniform（4112）、Sod AMR（3046）、Sedov AMR（3933）再次全部 PASS，`param.ini` 恢复。状态：**完成**。

#### M1.3 solver/coordinate 枚举门控

- **实施**：增加显式 `solver_type` 与 `coord_type` 判定接口，先保留旧比较作诊断对照，再切换调用。
- **专项验收**：每种合法组合均进入预期分支；不依赖枚举底层整数碰巧相等；通过 G1。

**完成记录（2026-08-03）**：起点提交 `0f44dcf`。审计确认原条件 `coord_type == RiemannSolver::GridAligned` 实际依赖 `plane == GridAligned == 0`，并未检查 solver 类型；默认黄金组合 `plane + Rotated` 证明该处设计意图是平面坐标门控。新增类型化 `CoordinateType`、`SolverType`、legacy 转换和 `should_run_riemann`，四种合法组合及非法整数范围专项测试 PASS；`plane` 下两种 solver 均执行 Riemann，`cylinder` 下均不执行，保持现有数值路径。跨枚举旧比较已被唯一显式门控替代，无临时诊断或额外旧分支可清理。G1 的 Noh Uniform（4112）、Sod AMR（3046）、Sedov AMR（3933）全部 PASS，`param.ini` 恢复。状态：**完成**。

#### M1.4 RK 阶段语义

- **实施**：先记录当前单阶段行为；根据设计依据决定“重命名为单阶段”或“实现完整 RK2”。两种方案不得混合推进。
- **专项验收**：阶段数、读写状态和接受解时机可测试；若实现 RK2，应建立新的、经确认的物理黄金基线，不能冒充旧黄金逐位回退。
- **说明**：在用户确认改变数值算法前，默认只做语义澄清和兼容封装，不改变现有数值路径。

**完成记录（2026-08-03）**：起点提交 `9b34708`。审计确认原 `two_stage_Runge_Kutta` 的循环恒为 `iter_num < 1`，第二阶段 `case 1` 永不可达，且仓库不存在第二阶段状态组合、Butcher 系数或阶数回归证据。按默认兼容方案新增单阶段 policy，明确 `stage_count=1`、`dt_scale(0)=1.0`、阶段 0 后接受；专项测试覆盖合法阶段和非法索引。主流程重命名为 `advance_single_stage`，删除循环、switch 和不可达半步分支，但保持半步变量、Riemann、状态更新、声速及外层 `AcceptNumericalSolution` 的顺序不变。G1 的 Noh Uniform（4112）、Sod AMR（3046）、Sedov AMR（3933）全部 PASS，`param.ini` 恢复。状态：**完成**；实现真正 RK2 必须另立算法里程碑并建立经批准的新物理基线。

#### M1.5 输出步与 refresh 语义

- **实施**：分别明确输出是步前还是步后；验证 `refresh_after_balance` 在无拓扑变化时的幂等性。
- **专项验收**：文件名、step、time、VTK `TimeValue` 一致；连续调用 refresh 不改变状态；通过 G1+G2。

**M1.5a 完成记录（2026-08-03）**：起点提交 `d2c435c`。审计确认现有输出位于物理推进和 `AcceptNumericalSolution` 之前，文件 `N` 保存已接受的 `N-1` 步状态，VTK `TimeValue` 对应该步前状态；本子区间不移动输出、不重命名文件、不改变 VTU 数值内容。新增类型化 `OutputStamp`，显式记录 `FileStep=N`、`StateStep=N-1`、`TimeValue=current_time`、`OutputPhase=PreStep(0)`；专项测试覆盖首步、后续步和零步边界。Sedov 末帧实测元数据为 `FileStep=3933`、`StateStep=3932`、`TimeValue=0.4995868398544829`、`OutputPhase=0`。G1 三黄金全部 PASS 且 `param.ini` 恢复；G2 step 3/4/10/50/54 全部 PASS。真正的步后/终态输出留作独立、需新输出契约的后续变更。状态：**完成**。

**M1.5b 完成记录（2026-08-03）**：起点提交 `e186712`。新增默认关闭的 `LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE` 专项开关；启用时在第一次 `refresh_after_balance` 后记录本 rank 全部 owner quadrant 与 ghost mirror 的 `quad_data_t` 字节快照，立即重复 refresh 并再次快照，逐字节比较后通过 `MPI_Allreduce(MIN)` 汇总所有 rank，任一差异都会中止。启用专项检查的完整 G1 中 Noh Uniform、Sod AMR、Sedov AMR 全部 PASS，且 `param.ini` 恢复；启用检查的 G2 step 3/4/10/50/54 全部 PASS，覆盖跨 rank 和 AMR 场景。随后删除逐步 PASS 日志、预留快照容量以降低诊断开销，并在默认关闭检查的生产路径再次执行 G1，三黄金全部 PASS（20.5s、61.5s、49.1s），`param.ini` 恢复。结论：在当前覆盖范围内，无拓扑变化时连续 refresh 对 owner 与 ghost 状态严格字节幂等；本子区间不改变 refresh 调用频率或数值算法。状态：**完成**。

**M1 完成条件**：M1.1～M1.5 各自有独立提交和 G1 证据，整体再通过 G1+G2+G3。

**M1 整体完成记录（2026-08-04）**：M1.1～M1.5 已分别完成并形成独立提交：时间步局部最小值 `e4ff042`、coarsen family 判据 `0f44dcf`、solver/coordinate 门控 `9b34708`、单阶段语义 `d2c435c`、输出戳语义 `e186712`、refresh 幂等验证 `a8f91f3`。整体闭环重新执行 G1，Noh Uniform（16.5s）、Sod AMR（61.7s）、Sedov AMR（49.2s）全部 PASS，且 `param.ini` 恢复；G2 step 3/4/10/50/54 全部 PASS；G3 四核 Sod AMR（30.6s）与四核 Sedov AMR（25.9s）均对并行黄金参考 PASS。状态：**M1 完成**，下一实施子里程碑为 M2.1 类型化配置和时钟。

### M2：类型化配置与状态访问

**目标**：先建立兼容层，不立即改变 `quad_data_t/CVariable` 的底层布局。

#### M2.1 类型化配置和时钟

- 新增 `SimulationConfig`、`MeshConfig`、`SolverConfig`、`OutputConfig`、`SimulationClock`；
- 从旧 `p4est_data_t` 只读映射，旧字段暂时保留；
- 调用点分批切换到类型化配置；
- **专项验收**：非法配置启动即失败；运行中配置不可写；通过 G1。

**完成记录（2026-08-04）**：起点提交 `f1d3067`。新增 `SimulationConfig`、`MeshConfig`、`SolverConfig`、`OutputConfig` 和 `SimulationClock` 值类型，由旧 `p4est_data_t` 单向生成只读快照，旧字段继续作为唯一写入源，未改变 `quad_data_t/CVariable` 布局。启动阶段首批切换初始网格层级以及时间循环起止时间，动态时间步归约、AMR callback、时钟递增和数值更新继续使用旧可变字段，避免双向状态与浮点顺序变化。新增统一配置/时钟校验，在 p4est 网格创建前拒绝非正层级/周期/输出间隔/时间步/CFL、反向时间区间和 NaN/Inf；专项测试覆盖 legacy 字段映射、clock 快照及非法边界。审计中补齐旧结构此前未初始化的 `dt_iter`、`used_dt`、边界类型和累计能量字段，均使用既有运行语义的中性初值。迁移后 G1 三黄金全部 PASS；清理后 G1 的 Noh Uniform（19.8s）、Sod AMR（61.5s）、Sedov AMR（48.8s）再次全部 PASS，且 `param.ini` 恢复。状态：**完成**。

#### M2.2 typed accessor

- 为 cell/edge/corner 与 current/half/lag 状态增加命名 accessor；
- 第一阶段 accessor 仍读写旧数组，保证内存布局不变；
- 按“热力学→几何→角点→边”的批次迁移调用；
- 建立 accessor 只读一致性测试（`python/test_variable_accessors.py`）：同一内存索引经 accessor 读写与旧裸数组逐字段一致；
- **专项验收**：每批 G0，子里程碑结束 G1。

**完成记录（2026-08-04）**：起点提交 `5c93613`（第一批：`cell/corner/edge/corner_vector` accessor + 热力学 `cell()` 批次）。本子里程碑继续补齐 `cell_vector/edge_vector/int_cell/int_edge` 四个 accessor 与一致性测试，并完成 `main.cpp` + `src/amr/amr_criteria.h` 的全量机械迁移（588 处 `数组[索引]`→accessor，覆盖热力学/几何/角点/边）。迁移后修复 typed accessor 暴露的 88 处动态 `int` 索引调用点（编译器 `invalid conversion from 'int' to <枚举>`）：① 字段选择器变量改类型化声明——`idCPara`→`DoubleCellVariableID`、`idEPara`→`DoubleEdgeVariableID`、`idCNPara`→`DoubleCornerVariableID`、`idCnIndex`→`VectorCornerVariableID`、`idCIndex`→`DoubleCellVariableID`；② `idChildIndex`（转移回调）按几何循环取 `VectorCornerVariableID`、物理循环取 `DoubleCellVariableID` 两个独立作用域分别声明；③ 循环计数器保留 `int`（C++14 枚举不支持 `++` 与 `0` 初始化），访问器调用处显式 `static_cast<枚举类型>`。另删除两处死代码赋值（`idCPara = idCentroidCoord_cur`：`Lagrangian_refine_fixed_estimate` 与 `RefineErrorEstimate`），该赋值将 `VectorCellVariableID` 混入 `DoubleCellVariableID` 但变量从未被读取（Distance 分支提前返回）。收口门禁：G0（`python/test_variable_accessors.py` PASS + 编译通过）；G1 的 Noh Uniform（4112）、Sod AMR（3046）、Sedov AMR（3933）全部 PASS 且 `param.ini` 恢复；G3 四核 Sod AMR（34.5s）与四核 Sedov AMR（27.3s）均对并行黄金参考 PASS。状态：**完成**。下一子里程碑为 M2.3 状态不变量检查。

#### M2.3 状态不变量检查

- 增加可开关的体积、质量、EOS、声速、质心和总能量检查；
- 检查器必须只读且不改变通信时序；
- **专项验收**：故意注入非法状态时可定位到稳定 CellKey；正常 G1 无误报。

**完成记录（2026-08-04）**：新增 `src/diagnostics/state_invariant_checker.h`（纯函数 `Diagnostics::check_cell_invariants`，检查体积>0、质量>0、密度>0、`密度=质量/体积`、`压力=EOS(gamma,密度,内能)`、`声速=c(gamma,p,ρ)`、`centroid_cur=GetPolyCenter(corner_cur)` 质心自洽、总能量>0），由 `LAGRANGIAN_CHECK_STATE_INVARIANTS` 环境变量开关（默认关闭），并在 main.cpp 的 p4est 适配器中经只读 `p4est_iterate(NULL ghost)` 遍历 owner 单元，违例时以稳定 CellKey `(treeid, level, x, y)` 报告并 abort。检查点：初始化后（phase 0）与 `AcceptNumericalSolution` 后（phase 1）。设计过程中两处不变量修正：① 质心必须用 **cur** 状态（`centroid_cur`/`corner_cur`）而非 lag——初始化只设 `CentroidCoordCur`，lag 在 phase 0 是零值；② 删除 `total≥internal` 假不变量——该求解器中 total 由守恒式 `total_half-dt·work/mass` 更新、internal 由 `internal_half-dt·(work-kineticVariation)/mass` 推导，两者仅全局一致，冲击/AMR 边界处 internal 可瞬时略超 total（实测 ~0.06%），仅保留 `total_energy>0`。专项验收：单元测试 `python/test_state_invariants.py` 用 8 组合法/注入状态验证纯函数定位违例名，PASS；真实运行中检查器已演示 CellKey 定位（如 `(tree=0, level=6, x=..., y=...)`）；启用检查的完整 G1 三黄金（Noh 22.7s / Sod 67.7s / Sedov 54.6s）全部 PASS 无误报，且与 `reference/` 1e-12 比对通过（证明检查器只读、不改结果），`param.ini` 恢复；未启用时 G1 三黄金同样 PASS。状态：**完成**。

- 仅在 M2.1～M2.3 全部通过后，删除无调用的重复字段和 accessor；
- 每一批删除都列出符号清单；
- **专项验收**：清理后 G1。

**完成记录（2026-08-04）**：先审计 M2.1～M2.3 后的残留直接访问与死代码。审计结论：① 业务代码对 `CVariable` 裸数组（`DouCData/DouCnData/DouEData/IntCData/VecCData/VecCnData/VecEdata`）的直接下标访问已**清零**（0 处），全部经由类型化 accessor；② `p4est_data_t` 的 `initial_dt/max_dt` 虽未被计算路径读取，但被 `simulation_config()` 快照读入 `SolverConfig.initial_timestep/maximum_timestep` 参与配置合法性校验，属半迁移字段，**保留**；③ `m_grid_info/last_output_index/shock_velocity` 等均有调用，保留。唯一确认无调用的死代码是 `IntEdgeVariableID` 系列：删除 `enum IntEdgeVariableID{idEdgeType,idIntEdgeVariableNum}`、`int IntEData[idIntEdgeVariableNum][CNDIM]` 数组、`int_edge()` 两个 accessor（含 `test_variable_accessors.py` 对应测试块）——边类型实际存储于 `CEdge_data.EdgeType`，该数组是重复且零使用的冗余。清理后 G0（编译 + `python/test_variable_accessors.py`）PASS；G1 三黄金全部 PASS，`param.ini` 恢复。状态：**完成**。

**M2 完成条件（2026-08-04 达成）**：业务代码不再直接使用已迁移的裸配置/状态索引——M2.2 后对状态裸数组的直接下标访问为 0；M2.1 已迁移的配置项（初始网格层级、时间循环起止）经 `SimulationConfig/SimulationClock` 读取，未迁移的动态项（时间步归约、AMR、时钟递增）按 M2.1 设计继续使用旧可变字段，底层布局替换留待后续独立里程碑。**M2 状态：完成**，下一里程碑为 M3 ghost 生命周期与通信契约。

### M3：显式化 ghost 生命周期和通信契约

**目标**：把 MPI 正确性依赖从调用约定变成 API 约束。

> **并行门禁**：M3.x 起各子里程碑修改 ghost/通信契约，并行正确性统一以 G3（4 核 Sod/Sedov 并行黄金）为准；原 G2 指定步数门禁已退役。

#### M3.1 全量 callback 通信审计

- 为所有 face/corner callback 建表：Requires、Reads、Writes、Invalidates、Exchange；
- 标记 local、remote snapshot、scratch、owner commit；
- **专项验收**：每个 `p4est_iterate` 均有条目，漏传 ghost 的接口为零；仅文档/诊断改动仍须 G1。

**完成记录（2026-08-04）**：产出 `docs/communication_audit.md`，盘点全部 51 个 `p4est_iterate` 调用，为 12 个活跃 face/corner callback 建表（类型、行号、调用点、ghost 状态、核心 Reads/Writes、Exchange 供给时序）并标注 local/remote/scratch。审计结论：① **漏传 ghost 的活跃接口为零**——全部活跃 face/corner callback 均以非空 ghost + `ghost_data` 调用；唯一 `NULL ghost` 的 face 调用位于从未执行的 `postprocess_after_coarsening` 死代码内。② **发现 4 个死符号**（仓库内零调用）：`postprocess_after_coarsening`、`quadrant_update_after_coarsening_callback`（且以 `NULL ghost_data` 注册但函数体解引用之）、`quadrant_update_parent_velo_press_callback`、`quadrant_vtk_coord_update_callback`，留待 M3.5/独立清理里程碑删除。③ **跨 rank 隐患（M3.4 依据）**：callback 1/5/7/8/9/11/12 经可写 `quad_data_t*` 修改可能指向 `ghost_data` 的单元，`ghost_exchange_data` 仅 owner→ghost 单向拷贝，ghost mirror 写静默丢失——与 §5.2「禁止把 ghost mirror 当成权威状态写入」一致。④ 完整映射 ghost 生命周期（拓扑变化后重建 + 各 phase 前交换，见文档 §3）。⑤ 附加发现：#7 `quadrant_set_init_parent_edge_callback` 内同一 `user_data` 双强转（`p4est_data_t*`/`quad_data_t*`），误转 inert；#8 `quadrant_get_children_hanging_info_callback` 拷贝的 `delta_u_cp/Uc_cur/Zcp` 在本步尚未刷新，为潜在 stale 隐患。门禁：本子里程碑仅文档改动，未改代码；编译通过，M2.4 的 G1 三黄金基线（`c40e2f2`）继续成立。状态：**完成**。下一子里程碑为 M3.2 `GhostSession` 兼容包装。

#### M3.2 引入 `GhostSession` 兼容包装

- 包装 create/exchange/destroy/generation，旧调用暂时从包装内部转发；
- topology 变化后令旧 generation 失效；
- **专项验收**：过期 ghost 在 Debug 构建中被检测；通过 G1。

**完成记录（2026-08-04）**：新增 `src/mesh/ghost_session.h`（`GhostSession` 类），包装 p4est ghost 生命周期：`initialize`（`p4est_ghost_new` + 用户数据缓冲 + 首次 `exchange`）、`destroy`（安全可空）、`rebuild`（destroy+initialize）、`invalidate_after_topology_change`、`exchange`、只读 `remote(ghost_id)`（M3.4 的 remote snapshot 接口）、以及 generation/topology_version 计数。**Debug 检测**：无 NDEBUG 构建（本项目 `-O2 -g`）下，对已失效 session 调用 `get/data/remote/exchange` 会触发 `assert` 中止。`advance_time_step` 的 ghost 生命周期（L5614 初始构建、AMR refine 后重建、coarsen+balance 后销毁、partition 后销毁、空时重建、步末交换、结束销毁）全部改由 `GhostSession` 管理，拓扑变化点显式调用 `invalidate_after_topology_change`；既有调用点（`PreProcess/set_allowing_coarsening_tag/refresh_after_balance/advance_single_stage` 及各 `p4est_iterate`）继续经 `get()/data()` 转发原始指针（兼容包装，调用点迁移留待 M3.3）。专项验收：单元测试 `python/test_ghost_session.py` 验证状态迁移（empty/valid/generation/topology_version）与**过期访问触发断言中止**（子进程非零退出），PASS。门禁：G1 三黄金（Noh 4112 / Sod 3046 / Sedov 3933）全部 PASS 且 `param.ini` 恢复；G3 四核 Sod AMR（29.6s）与四核 Sedov AMR（24.9s）均对并行黄金参考 PASS。状态：**完成**。下一子里程碑为 M3.3 分阶段迁移 ghost 调用点。

- 按 Gradient/AMR、balance refresh、Corner/Riemann、force/update 分批迁移；
- 每批保留旧入口，使用新入口转发；
- **专项验收**：每批 G0+相关短步，整体 G1。

**完成记录（2026-08-04）**：完成 ghost 调用点的兼容迁移：Gradient/AMR（`Gradient_estimate`、`PreProcess`、`set_allowing_coarsening_tag`）、balance refresh（`refresh_after_balance` 与幂等快照）、Corner/Riemann（`MatrixAssemble`、`ComputeCornerNodeVelocity`、`ComputeHangingNodeVelocity...`、`RiemannSolver`）和 AMR boundary（`Get_AMR_BDY_info`）均改为接收 `GhostSession&`，不再在业务函数签名中传递裸 `p4est_ghost_t*` / `void* ghost_data`。所有内部 `p4est_ghost_exchange_data` 调用改为 `session.exchange()`；所有 iterate ghost 参数改为 `session.get()/session.data()`；不需要 ghost 的 volume callback 显式保持 NULL。`advance_time_step` 只保留一个 `GhostSession` 局部对象，不再维护原始 ghost/data 别名，拓扑变化调用 invalidate+destroy/rebuild。`Predict_refining_Quads` 只接受 session 但本身为零调用死函数，保留到 M3.5 清理。门禁：G0 编译 + `test_variable_accessors.py`、`test_state_invariants.py`、`test_ghost_session.py` 全部 PASS；G1 三黄金（Noh 4112 / Sod 3046 / Sedov 3933）全部 PASS 且 `param.ini` 恢复；G3 四核 Sod AMR（28.1s）与 Sedov AMR（23.5s）均对并行黄金参考。状态：**完成**。下一子里程碑为 M3.4 remote 只读和 owner commit。

**锚点重启规则（2026-08-04）**：返回最近一次 G0–G3 全部通过的已验证锚点后，未知中间态不得继续作为活动开发基线；M3.4 到后续里程碑之间未能证明根因已定位的活动工作链路应丢弃，重新细分并从锚点推进。失败中间态只能作为明确标记、可恢复的取证存档，不能恢复后继续叠加，除非后续调查已隔离并证明其根因。锚点本身没有发生的故障，不应被当作重启后的活动故障；只有新修改再次触发失败时，才分析该新修改引入的回归。状态：**执行中**。

- 引入 `RemoteCellSnapshot` 与 scratch；
- 逐 callback 消除对 ghost mirror 的权威写入；
- **专项验收**：1/2/4 ranks 改变不影响规定步数结果；通过 G1+G3。

#### M3.5 通信旧路径瘦身

M3.5 只允许在 M3.4 通过当前完整 G0～G3 重跑后启动。清理对象必须先有全仓库零调用证据；活动 ghost 生命周期、`GhostSession::get()/data()`、callback context、`corner_solver.h` 兼容声明和 owner/remote 访问路径不得因“看起来旧”而删除。

为降低通信重构的回归半径，M3.5 分为以下十个可独立回退的阶段。每个阶段都必须按本节 §2.4 的工作流执行；G0～G3 全部通过后，才能记录该阶段完成并创建 focused commit：

| 阶段 | 范围 | 必须保持不变或确认的内容 |
|---|---|---|
| R0 | 重现入口锚点和完整门禁 | commit、工具链、MPI runtime、参数 checksum、G0～G3 证据 |
| R1 | 隔离 G3 失败 | 重复 canonical G3，记录失败 step、能量误差、rank、partition、exit code；不改生产代码 |
| R2 | 确认通过基线和清理契约 | 只有 G3 两算例都通过才可进入；列出零调用候选与 deferred 活动路径 |
| C1 | 删除 `GhostSession::data_size_` | 不改变 ghost allocation、exchange、generation 和 validity |
| C2 | 删除零调用 VTK 坐标 callback | 不改变 VTK writer、字段、文件名和时间元数据 |
| C3 | 删除零调用 prediction path | 不改变 active refine criteria、AMR tag 和默认标记 |
| C4 | 删除零调用 post-coarsening path | 不改变 active coarsen、balance、partition、refresh 和 rebuild 时序 |
| C5 | 删除零调用 parent velocity/pressure callback | 不触碰 active parent update、Riemann、corner、hanging 或 exchange |
| C6 | 全量符号、调用和 ownership 审计 | 确认五组清理符号无残留，活动 ghost API/context 仍保留 |
| C7 | 最终 M3.5 收口 | clean build、完整 G0/G1/G3，G3 连续两次，参数和参考文件不变 |

其中 R0～R2 是基线和原因隔离阶段，C1～C7 是清理阶段；R1 未闭合时禁止进入 R2 或任何 C 阶段。若 MPI rollback 失败，停留在最近一个可靠 G0～G3 锚点，不把诊断 executable、日志、summary 或未知工作树状态升级为基线。

**旧状态记录（2026-08-05，已被后续实测取代）**：当时曾记录 canonical 四进程 G3 Sod AMR 失败、Sedov 因首项失败而未执行。该记录只描述当时的环境/提交状态，不能作为当前 M3.5 门禁结论。2026-08-06 在 `bed5156` 基线上重新执行 R0：G0 clean build、G1 Noh/Sod/Sedov、G3 四进程 Sod/Sedov 均实际通过，`param.ini` 恢复为真；因此 R0 已闭合，当前进入 R1 清理契约审计，C1～C7 仍须按顺序逐阶段执行，不得用 R0 证据替代各清理阶段门禁。

**M3.5 延迟清理项（转入 M4）**：
为了绝对控制风险，以下仍在活跃执行的通信相关操作被严格剔除出 M3.5，统一推迟至 M4 解决：
1. `ghost_data` 裸数组指针别名（如 `ghost_data[quadid]`）向 `GhostSession::remote()` 的安全迁移。
2. Edge/Corner/Hanging 回调中针对幽灵层“仅为写入 mirror 而执行”的冗余浮点计算短路。
3. 全局 `void *ghost_data` 等本地占位符声明的彻底根除。
### M4：AMR 子系统模块化

**目标**：AMR 决策、transfer、悬点修复可分别测试，p4est callback 仅适配数据。同时安全清算 M3.5 遗留的高风险通信瘦身项。

#### M4.0 剥离残留通信旧路径 (M3.5 遗留项)

- **替换裸指针读取**：分批次（AMR判定/Edge/Corner/Hanging）将所有针对 `ghost_data[quadid]` 的裸数组解引用，安全迁移至强类型的 `GhostSession::remote(quadid)`，实现强制只读。
- **短路死计算**：在核心回调中，把 `!is_ghost` 的“写入前拦截”升级为“计算前拦截”，跳过仅服务于幽灵层的庞大浮点矩阵和通量计算。
- **清理本地占位符**：全局抹除 `void *ghost_data = context->session->data();` 等所有绕过封装的局部环境声明。
- **专项验收**：每次指针替换和计算短路均属于极高危操作，必须强制闭环执行完整 G0~G3；并行黄金结果（特别是网格拓扑、边界条件和守恒量）不得出现偏差。

#### M4.1 抽取 `AMRPolicy`

- 先把现有判据原样搬入纯接口，不改变算法；
- 建立 refine/coarsen、level、阈值和 sibling 顺序测试；
- **专项验收**：旧/新 policy 对完整测试样本逐项同判；G1。

#### M4.2 抽取 `AMRTransfer`

- 抽取 parent→children 与 children→parent；旧 callback 保留为 adapter；
- **专项验收**：质量、动量、总能量、体积守恒；G1。

#### M4.3 抽取 hanging repair

- 对比 balance/coarsen 两套 callback 的逐字段行为；
- 先统一到 `enforce_hanging_consistency`，确认覆盖后再删除重复实现；
- **专项验收**：幂等性、coarse-fine 几何与速度约束、G1。

#### M4.4 建立 AMR controller

- 统一 refine→exchange→coarsen→balance→partition→rebuild ghost 的阶段编排；
- **专项验收**：拓扑 generation 和 exchange 次序可追踪；G1+G3。

#### M4.5 AMR 旧代码瘦身

- 删除 `main.cpp` 中已被 controller/policy/transfer 覆盖的实现；
- **专项验收**：删除清单可审计，清理后 G1。

### M5：Hydro/Riemann 子系统模块化

**目标**：显式分离角点贡献、主点求解、悬点约束、力组装和守恒更新。

#### M5.1 抽取纯角点数学

- 抽取矩阵组装与 2×2 求解；保留旧函数作对照；
- **专项验收**：正常、近奇异、边界条件样本逐项一致；G1。

#### M5.2 phase 化 Riemann 调用链

- 建立 reset→assemble→exchange→solve master→exchange→solve hanging→exchange→force；
- 每阶段声明输入、输出和同步边界；
- **专项验收**：阶段锚点与旧路径逐字段一致；G1。

#### M5.3 明确共享角点确定性策略

- 选择 owner 求解或固定排序重复求解；
- 禁止依赖 rank、本地 `quadid` 或不稳定累加顺序；
- **专项验收**：1/2/4 ranks 的共享角点结果满足既定一致性；G1+G3。

#### M5.4 抽取守恒更新与状态接受

- 分离 density、momentum、work、energy、EOS、sound speed、acceptance；
- **专项验收**：单步阶段对照、守恒量、不变量和 G1。

#### M5.5 Hydro 旧代码瘦身

- 新模块完全接管后删除 `main.cpp` 中重复实现；
- **专项验收**：删除前后各 G1，删除后 G3。

### M6：Mesh adapter、IO 与 Diagnostics 拆分

#### M6.1 稳定 CellKey 与 p4est adapter

- 统一 `(treeid, level, x, y)`；业务层不再直接转换 `void *`；
- **专项验收**：串并行日志和比较器能稳定对齐；G1。

#### M6.2 IO 模块

- 迁移 VTK/PVTU/profile writer 与输出调度；先转发旧实现；
- **专项验收**：文件命名、字段、精度和时间元数据与旧路径一致；G1。

#### M6.3 Diagnostics 模块

- 迁移守恒监控、不变量、checksum、定点 trace；
- **专项验收**：默认关闭时无额外全场遍历和文件 IO；打开时不改变数值结果；G1。

#### M6.4 清理旧 IO/诊断代码

- 确认新模块覆盖后删除旧路径和硬编码 step/坐标 trace；
- **专项验收**：清理后 G1。

### M7：基础数学、构建系统与最终主程序

#### M7.1 基础数学兼容迁移

- `CDoubleVector/Matrix` 先适配到 `Vec2/Mat2`；逐调用点替换非标准运算符；
- **专项验收**：纯数学单测、G1；不得同时改变数值容差策略。

#### M7.2 拆分 Geometry/Physics

- 从 `alg.cpp/.h` 分批迁移纯函数，每批保留兼容入口；
- **专项验收**：旧/新函数样本对照、G1。

#### M7.3 统一构建系统

- 先让 CMake 与 Makefile 编译同一源文件集合和 C++ 标准；
- 两套构建结果通过后，再决定唯一正式入口，不能先删除 Makefile；
- **专项验收**：两种构建各完成 G1，最终入口再完成 G1。

#### M7.4 `Simulation::run()` 编排与 `main.cpp` 瘦身

- 先新增高层编排并让 `main.cpp` 转发；
- 模块全部接管后再删除旧实现；
- **专项验收**：`main.cpp` 只保留启动；G1+G3 全通过。

**M7 完成条件**：满足第 16 节 Definition of Done，并形成最终基线提交。

---

## 14. 不建议优先进行的工作

以下工作视觉收益明显，但当前不应优先：

1. 先把 `main.cpp` 按行数机械拆成多个 `.cpp`；
2. 全局一次性重命名所有变量和函数；
3. 在 ghost 语义未明确前引入复杂模板或通用 callback 框架；
4. 在并行一致性未形成明确策略前声称 MPI 完全确定；
5. 同一提交同时修改算法、数据布局、目录结构和构建系统；
6. 只以“程序能运行”作为验收，不运行串行三黄金回退；
7. 为追求性能减少 ghost exchange，却没有先写清每次 exchange 的数据依赖；
8. 在新旧实现尚未完成对照和 G1 前删除旧代码；
9. 把代码瘦身与功能迁移混在同一未经验证的提交中。

---

## 15. 建议立即开始的执行队列

按依赖关系，第一轮只推进以下任务：

1. **M0.1**：核实 `validate_current.ps1` 是否真正固定并执行三黄金配置，统一摘要与退出码；
2. **M0.2**：把 4 核参考整理成明确 G3 命令；
3. **M0.3**：建立子里程碑验收记录模板；
4. 完整执行一次 G1+G3，形成 M0 基线提交；
5. **M1.1**：只处理 timestep rank 内最小值；
6. M1.1 清理并闭环后，再进入 **M1.2 coarsen family 判据**。

在 M0 完成前，不启动 typed state、GhostSession、AMR/Hydro 文件拆分。M1 各项不得并行混改；每项都先保留旧路径、独立验证、再清理瘦身。

---

## 16. 重构完成定义（Definition of Done）

只有同时满足以下条件，才视为架构重构完成：

- `main.cpp` 不再承载具体数值算法；
- p4est/MPI 类型被限制在 runtime/mesh adapter 层；
- 物理、几何、AMR policy 和 transfer 可脱离 p4est 单测；
- ghost 生命周期、读写权限和 exchange 边界由 API 表达；
- 不再通过裸 `void *` 在业务层传递多种上下文；
- current/half/candidate 状态边界清晰；
- 配置无隐藏默认继承，回归可重现；
- 每个子里程碑的迁移完成点与清理完成点均有 Noh、Sod AMR、Sedov AMR 三黄金回退记录；
- 4 核 Sod/Sedov 并行黄金（G3）通过，与 `reference/par4_*` 一致；
- refine/coarsen 守恒、状态不变量和拓扑一致性均有自动测试；
- 构建系统唯一、可重复，源目录不再被构建输出污染。

### M3.4 小锚点验证记录（2026-08-04）

- A1：edge minmod callback 改用显式 `GhostCallbackContext`，通过 G0、G1、serial rollback 和 G3 四进程 MPI rollback；`param.ini` 字节恢复。
- A2：仅处理 conforming full-face 分支：远端 side 通过 `GhostSession::remote()` const read，owner-local side 才允许写入 gradient；通过最终直接编译、G0、G1 Noh/Sod/Sedov、serial rollback Noh/Sod/Sedov、G3 Sod/Sedov 和 MPI rollback；`mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。
- A2 已验证为当前 M3.4 活动锚点；hanging 分支、ghost ID 域检查和其他 callback 未混入本片，M3.4 仍未整体关闭，M3.5 不得开始。
- A3：仅处理同一 edge minmod callback 的 hanging branch：child/parent 通过 `GhostSession::remote()` 做 const read，gradient 仅经 owner-local write pointer 写回；通过编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `param.ini` 字节恢复。A3 已验证为当前 M3.4 活动锚点；ghost ID validity、corner data、其他 callback 和 hanging matrix 未混入本片。
- B1：仅将同一 edge minmod callback hanging branch 的远端 child ghost-ID 范围检查替换为 `GhostSession::valid_remote_id()`；本地 child ID 不走远端域检查，full-face 旧 guard、geometry、corner data、其他 callback 和 hanging matrix 未混入本片。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B1 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B2：仅处理 `quadrant_whether_allowing_coarsening_from_edge_callback` 的 full parent 写入：ghost parent 不再被当作可写镜像，直接跳过；owner-local parent 保留原有 `idAllowCoarsening` 标量写入。未改变 level 判定、geometry、corner data、hanging matrix、parent-edge 或其他 callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B2 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B3：仅处理 `quadrant_update_after_balance_callback` 的 hanging ghost-ID guard：远端 child ID 仅在对应 `is_ghost` 标志为真时通过 `GhostSession::valid_remote_id()` 校验，本地 child ID 不再与 `global_num_quadrants` 比较；所有 geometry、corner data、源数据读写和其他 callback 未混入本片。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B3 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B4：仅将 `quadrant_relaxed_hanging_solver_callback` 的 `user_data` 解码和调用点改为显式 `GhostCallbackContext`；未改变 child velocity、relaxed flux、parent-edge state、geometry、corner data 或数值写入路径。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B4 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B5：仅将 `quadrant_relaxed_hanging_solver_callback` 的 hanging ghost ID guard 从 `global_num_quadrants` 比较改为仅对 `is_ghost` child 使用 `GhostSession::valid_remote_id()`；未改变 child velocity、relaxed flux、parent-edge state、geometry、corner data 或数值写入路径。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `param_restored: true`。B5 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B6：仅在 `quadrant_relaxed_hanging_solver_callback` 中为 child/parent `CVariable` 增加 const read aliases，并将 master velocity 与 total-energy 读取切换到 const overload；原 mutable aliases 继续承担 child velocity、relaxed flux 和 parent-edge 写入，未改变 geometry、corner data 或数值公式。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B6 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B7：仅为 `quadrant_relaxed_hanging_solver_callback` 的 child velocity、child relaxed-flux 和 parent-edge state 写入增加 owner-local `is_ghost` 门禁；远端 child/parent 仍可作为只读计算输入，未改变 hanging matrix、points、geometry、corner data、exchange 时序或数值公式。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B7 已验证为当前 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B8a：仅为 `quadrant_corner_minmod_estimate_callback` 的两个 corner-gradient 目标写入增加 owner-local `is_ghost` 门禁；保留 local/ghost pressure、density、centroid 读取，未改变梯度公式、side 遍历顺序、`Gradient_estimate` 调用点、user-data 形态或 exchange 时序。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B8a 已验证为 M3.4 活动锚点；此前失败的 coarsening-corner B8 保持为已回退证据，M3.4 仍未整体关闭，M3.5 不得开始。
- B8b：仅为 `quadrant_corner_to_point_matrix_assemble_callback` 的 `MatrixP`、`RHS` 和 boundary `TwoBouns` 四个目标写入增加 owner-local `is_ghost` 门禁；保留 local/ghost matrix、RHS、boundary 输入读取，未改变矩阵聚合、边界判定、side 遍历顺序、回调调用点或 exchange 时序。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B8b 已验证为 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B8c：仅为 `quadrant_corner_velocity_callback` 的 boundary/interior velocity solve、`velo_lag` 归零归一化和 `idcnVelocity_lag` corner-vector 五类目标写入增加 owner-local `is_ghost` 门禁；保留 local/ghost MatrixP、RHS、TwoBouns 和 boundary flag 读取，未改变矩阵求解公式、trace 条件、side 遍历顺序、回调调用点或后续 exchange 时序。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B8c 已验证为 M3.4 活动锚点；M3.4 仍未整体关闭，M3.5 不得开始。
- B9：仅为 `quadrant_update_after_balance_callback` 的 hanging child 1/2 几何、速度、体积、密度和压力更新增加对应 `is_ghost` owner-local 门禁；保留 midpoint/delta-velocity 判定、全部 local/remote 输入读取、原有公式、side 顺序、回调调用点和 exchange 时序，未修改此前导致 MPI Sedov 拓扑回归的 coarsening-corner callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B9 已验证为 M3.4 活动锚点；剩余 hanging matrix、parent-edge 和 children-info 写入路径仍未闭合，M3.4 仍未整体关闭，M3.5 不得开始。
- B10（2026-08-05）：仅为 `quadrant_hanging_point_matrix_assemble_callback` 的两个 hanging child `MatrixP`/`RHS` 目标写入增加对应 `is_ghost` owner-local 门禁；保留矩阵/RHS 聚合、local/remote 输入读取、边界状态计算、side 顺序、回调调用点和 exchange 时序，未修改同一 callback 中的 hanging boundary metadata 写入。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B10 已验证为 M3.4 活动锚点；hanging boundary metadata、parent-edge 和 children-info 写入路径仍未闭合，M3.4 仍未整体关闭，M3.5 不得开始。
- B11（2026-08-05）：仅为 `quadrant_hanging_point_matrix_assemble_callback` 的两个 hanging child point 状态组（`IsHanging`、`TwoBouns`、`BounParent`、`master_coord_relaxed`、`hanging_coord`）增加对应 `is_ghost` owner-local 门禁；保留边界数据读取、矩阵/RHS 聚合、既有公式、side 顺序、回调调用点和 exchange 时序，未修改 `quadrant_set_init_parent_edge_callback` 或 `quadrant_get_children_hanging_info_callback`。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B11 已验证为 M3.4 活动锚点；parent-edge 和 children-info 写入路径仍未闭合，M3.4 仍未整体关闭，M3.5 不得开始。
- B12（2026-08-05）：仅为 `quadrant_set_init_parent_edge_callback` 的 full-parent `PCInfo[parent_face_index]` 发布写入增加 `is.full.is_ghost` owner-local 门禁；保留 child/parent 输入读取、`Lcp`/`Ncp` 聚合、`Hanging_velocity` 与既有公式、side 顺序、回调调用点和 exchange 时序，未修改同一 callback 中的 parent corner half-edge `Lcp` 写入，也未修改 `quadrant_get_children_hanging_info_callback` 或此前导致 MPI Sedov 拓扑回归的 coarsening-corner callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B12 已验证为 M3.4 活动锚点；parent corner half-edge 和 children-info 写入路径仍未闭合，M3.4 仍未整体关闭，M3.5 不得开始。
- B13（2026-08-05）：仅为 `quadrant_set_init_parent_edge_callback` 中 full-parent corner half-edge 的 `m_plus->Lcp`/`m_minus->Lcp` switch 写入增加 `is.full.is_ghost` owner-local 门禁；保留四个 parent-face 分支、master/hanging 坐标读取、距离公式、side 顺序、回调调用点和 exchange 时序，未修改 `PCInfo` 发布、`quadrant_get_children_hanging_info_callback` 或此前导致 MPI Sedov 拓扑回归的 coarsening-corner callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B13 已验证为 M3.4 活动锚点；children-info 写入路径仍未闭合，M3.4 仍未整体关闭，M3.5 不得开始。
- B14（2026-08-05）：仅为 `quadrant_get_children_hanging_info_callback` 的两个 hanging child point 状态组（`IsHanging`、`TwoBouns[0]`、`TwoBouns[1]`）增加对应 `is_ghost` owner-local 门禁；保留 boundary input 聚合、`Ncp`/`Lcp`/`delta_u_cp`/`Uc_cur`/`Zcp` 读取、既有值映射、side 顺序、回调调用点和 exchange 时序，未修改 parent-edge callbacks 或此前导致 MPI Sedov 拓扑回归的 coarsening-corner callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、G3 四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B14 已验证为 M3.4 活动锚点；需要进行最终 ghost-write 全量审计与收口门禁，M3.4 仍未整体关闭，M3.5 不得开始。
- B15（2026-08-05）：仅为 `quadrant_whether_allowing_coarsening_from_corner_callback` 的 `idAllowCoarsening` 写入增加 `!is_ghost_a` owner-local 门禁；保留 ghost/local quadrant 的层级比较、双重 side 遍历、coarsening 判定和 callback 注册/时序，未跳过 ghost side，未修改此前导致 MPI Sedov 拓扑回归的 `continue` 实验或其他 callback。通过直接编译、G0、serial rollback Noh/Sod/Sedov、四进程 MPI Sod/Sedov，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。B15 已验证为 M3.4 活动锚点；需继续执行最终 ghost-write 全量审计与 M3.4 收口门禁，M3.5 仍不得开始。
- M3.4 收口（2026-08-05）：B15 后对全部活动 `p4est_iterate` ghost callback 进行最终 ghost-write 审计，确认所有可写 quadrant 数据均限制为 owner-local 目标；未发现活动 blocker。B15 候选通过直接编译、G0、serial Noh/Sod/Sedov rollback、四进程 MPI Sod/Sedov rollback，且 `mpi_gate_summary.json` 报告 `status: PASS`、`param_restored: true`。据此 M3.4 已闭合，M3.5 可在本收口提交后启动。

### 当前版本门禁复核记录（2026-08-05）

当前已合并的 B15 提交 `744145e` 已完成 M3.4 B9→B15 的逐阶段收口：

- G0：Makefile clean build 与链接通过；
- G1：Noh Uniform、Sod AMR、Sedov AMR 全部通过，比较容差为 `1e-12`，`param.ini` 逐字节恢复；
- G3：四进程 Sod AMR、Sedov AMR 均实际执行并通过，solver/compare 退出码均为 `0`，`mpi_gate_summary.json` 状态为 `PASS`，`param_restored` 为 `true`；
- 每个 B9→B15 通过阶段均有独立源码提交和 `docs/golden-gates-b*.md` 详细记录；
- G2 保持 retired；reference 黄金资产未修改，运行输出和构建产物不纳入版本控制。

因此 M3.4 已按当前 B15 代码和机器摘要闭合；后续 M3.5 工作必须继续遵守完整 G0/G1/G3 门禁和失败即停规则。
