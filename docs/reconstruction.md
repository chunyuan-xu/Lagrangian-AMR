# Lagrangian-AMR 程序重构提纲

> 目标：在不破坏现有数值结果和 p4est/MPI 并行能力的前提下，把当前“以 `src/main.cpp` 和大量 `p4est_iterate` callback 为中心”的实现，逐步重构为职责清晰、通信契约显式、数值内核可单测、串并行可回归的模块化架构。
>
> 本文是重构路线提纲，不建议一次性大改。每一阶段都必须在独立提交中完成，并通过串行锚点与 MPI 一致性门禁后再进入下一阶段。

---

## 1. 当前架构概览

### 1.1 现有文件与职责

| 文件/目录 | 当前主要职责 | 主要问题 |
|---|---|---|
| `src/main.cpp` | 程序入口、初始化、主时间循环、AMR、ghost 生命周期、Riemann 求解、悬点处理、状态更新、输出、诊断 | 职责高度集中；约 50 个 `p4est_iterate` 调用；算法依赖回调注册顺序和隐式通信时序 |
| `src/variable.h` | 通过枚举索引和裸数组保存单元、边、角点、当前/半步/滞后状态 | 字段缺乏类型语义，错误索引可编译通过；状态阶段容易部分更新 |
| `src/defines.h` | 配置、枚举、运行时状态、输出文件流 | 配置、运行状态和 IO 资源混合；大量 `int` 代替强类型枚举；构造函数包含文件副作用 |
| `src/alg.cpp/.h` | 几何算法、物理公式、初始/边界条件 | 几何、物理和算例初始化边界仍可继续拆分 |
| `src/amr/amr_criteria.h` | p4est refine/coarsen 判据 | policy 直接依赖 p4est 与完整单元结构，难以单测；coarsen sibling 逻辑需优先核验 |
| `src/core/vector_matrix.h` | 二维向量和矩阵 | 非标准 `operator--` 表示取负，`operator^` 表示点积；判等容差硬编码为 `1e-100` |
| `src/io/config_parser.*` | 读取 ini 配置 | 已具备基础边界，但配置校验与默认值仍在 `p4est_data_t` 中 |
| `src/io/vtk_writer.h` | VTK 输出声明/薄封装 | 大量实际输出和调试逻辑仍位于 `main.cpp` |
| `src/solver/corner_solver.h` | 角点矩阵和速度求解声明 | 主要实现仍位于 `main.cpp`，模块尚未真正落地 |
| `src/physics/eos.h` | EOS 纯函数 | 方向正确，但被状态头文件不必要地包含 |
| `python/quick_consistency_test.py` | 指定步数串行/MPI 快速一致性测试 | 可作为重构期间的 MPI 快速门禁 |
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
      ├─ refresh_after_balance
      ├─ ghost exchange
      ├─ predict_timestep + MPI_Allreduce(MIN)
      ├─ write_solution（当前位于物理推进之前）
      ├─ two_stage_Runge_Kutta（当前循环仅执行一次）
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
      ├─ StatTotalEnergyError
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

三算例必须使用固定黄金配置，并与 `reference/` 中对应参考解比较；不能用短步冒烟、编译成功或单个算例通过代替三黄金回退。MPI 快速门禁、单元测试和静态检查是附加门禁，不能替代三黄金回退。

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

### 2.3 每个子里程碑的统一执行模板

每个子里程碑按以下七步执行：

1. **范围冻结**：写清目标、非目标、涉及文件和预期不变的数值行为；
2. **前置基线**：在修改前运行三黄金回退，确认起点可信；
3. **兼容实现**：新增接口或模块，保留旧实现，避免混入无关重命名和优化；
4. **调用迁移**：按小批次切换调用点，每批至少编译并运行相关短程测试；
5. **主验收**：运行完整 Noh/Sod AMR/Sedov AMR 三黄金回退；
6. **清理瘦身**：仅删除已证明无调用、行为已被新实现覆盖的旧代码，再次运行三黄金回退；
7. **独立提交**：记录变更范围、三算例结果、专项测试、删除清单和已知风险。

若第 5 或第 6 步失败，子里程碑不得完成；优先恢复到上一个通过三黄金回退的提交，再定位问题。

### 2.4 每次提交的边界

每个重构提交应满足：

- 只处理一个概念，如“封装 ghost 生命周期”或“抽取 AMR criterion”；
- 不同时进行算法修复和大规模重命名；
- 子里程碑收口提交必须附 Noh/Sod AMR/Sedov AMR 三黄金回退结果；
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
4. 固定 MPI 快速门禁：
   - step 3、4、10、50、54；
   - 当前已知 step 55 分歧作为“预期失败”或单独问题跟踪；
   - 至少覆盖 MPI 1/2/4 ranks。
5. 保存关键守恒量与字段 checksum：质量、动量、总能量、网格数量、稳定网格键集合。

#### 验收标准

- 单条命令完成全部串行锚点和 MPI 门禁；
- 测试结束后配置必定恢复；
- 测试结果不依赖调用者当前 `param.ini` 的残留值；
- 失败能区分：运行失败、拓扑不一致、字段不一致、环境缺失。

### 3.2 核验并处理 P0 数值风险

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
- step 55 临界阈值问题可重现并有明确策略（滞回、安全带或确定性归约）。

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
5. **长期回归**：step 50/54/55/100 与守恒量。

---

## 11. P3：统一构建系统与工程规范

当前 Makefile 使用 C++14，并编译 `config_parser.cpp`；CMake 使用 C++11，且 `AMR_Solver` 源文件列表没有包含 `config_parser.cpp`。建议：

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

为避免“完成”的含义随阶段变化，统一使用四级门禁：

| 门禁 | 内容 | 使用时机 |
|---|---|---|
| G0 | 编译、静态检查、相关单元测试 | 每个小批次调用迁移后 |
| G1 | 完整串行 Noh、Sod AMR、Sedov AMR 黄金回退 | **每个子里程碑前后，以及清理瘦身后；强制** |
| G2 | MPI step 3/4/10/50/54，按 `(treeid, level, x, y)` 比较 | 涉及 mesh、ghost、AMR、Hydro、状态布局时 |
| G3 | 4 核 Sod/Sedov 并行黄金、守恒量和负载合理性 | 每个大里程碑结束及最终发布前 |

G1 是最低完成门槛。任何子里程碑不得因 G0、G2 或 G3 通过而跳过 G1。

### M0：冻结可信基线与执行协议

**目标**：让后续每次重构都有可重复、不会污染配置的统一裁判。

#### M0.1 固化串行三黄金门禁

- **实施**：统一 `validate_current.ps1`、`python/run_tests.py`、`python/_anchor_regression.py` 的职责和退出码；明确黄金配置与参考文件映射。
- **产物**：单条命令、三算例摘要、失败分类、配置恢复证明。
- **验收**：连续运行两次 G1，结果相同，`param.ini` 与运行前逐字节一致。

#### M0.2 固化 MPI 快速与长期门禁

- **实施**：把 step 3/4/10/50/54 固定为 G2；把 step 55 临界阈值现象作为独立已知问题；固定 4 核 Sod/Sedov 为 G3。
- **产物**：机器可读 summary，明确 PASS、预期差异和真实失败。
- **验收**：G1、G2、G3 均可由文档命令重复执行。

#### M0.3 建立子里程碑记录模板

- **实施**：每个子里程碑记录范围、前置 G1、迁移批次、专项测试、后置 G1、删除清单、清理后 G1、提交号。
- **产物**：统一验收记录格式。
- **验收**：后续任务不能在缺少这些证据时标记完成。

**M0 完成条件**：M0.1～M0.3 全部完成，并通过 G1+G2+G3。

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
- G2：PASS/FAIL/不适用（列出 step）
- G3：PASS/FAIL/不适用（列出算例）

### 清理瘦身
- 删除符号/文件清单：
- 无调用证明：
- 清理后 G1：PASS/FAIL
- 清理后 G2/G3：PASS/FAIL/不适用

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

#### M1.3 solver/coordinate 枚举门控

- **实施**：增加显式 `solver_type` 与 `coord_type` 判定接口，先保留旧比较作诊断对照，再切换调用。
- **专项验收**：每种合法组合均进入预期分支；不依赖枚举底层整数碰巧相等；通过 G1。

#### M1.4 RK 阶段语义

- **实施**：先记录当前单阶段行为；根据设计依据决定“重命名为单阶段”或“实现完整 RK2”。两种方案不得混合推进。
- **专项验收**：阶段数、读写状态和接受解时机可测试；若实现 RK2，应建立新的、经确认的物理黄金基线，不能冒充旧黄金逐位回退。
- **说明**：在用户确认改变数值算法前，默认只做语义澄清和兼容封装，不改变现有数值路径。

#### M1.5 输出步与 refresh 语义

- **实施**：分别明确输出是步前还是步后；验证 `refresh_after_balance` 在无拓扑变化时的幂等性。
- **专项验收**：文件名、step、time、VTK `TimeValue` 一致；连续调用 refresh 不改变状态；通过 G1+G2。

**M1 完成条件**：M1.1～M1.5 各自有独立提交和 G1 证据，整体再通过 G1+G2+G3。

### M2：类型化配置与状态访问

**目标**：先建立兼容层，不立即改变 `quad_data_t/CVariable` 的底层布局。

#### M2.1 类型化配置和时钟

- 新增 `SimulationConfig`、`MeshConfig`、`SolverConfig`、`OutputConfig`、`SimulationClock`；
- 从旧 `p4est_data_t` 只读映射，旧字段暂时保留；
- 调用点分批切换到类型化配置；
- **专项验收**：非法配置启动即失败；运行中配置不可写；通过 G1。

#### M2.2 typed accessor

- 为 cell/edge/corner 与 current/half/lag 状态增加命名 accessor；
- 第一阶段 accessor 仍读写旧数组，保证内存布局不变；
- 按“热力学→几何→角点→边”的批次迁移调用；
- **专项验收**：每批 G0，子里程碑结束 G1+G2。

#### M2.3 状态不变量检查

- 增加可开关的体积、质量、EOS、声速、质心和总能量检查；
- 检查器必须只读且不改变通信时序；
- **专项验收**：故意注入非法状态时可定位到稳定 CellKey；正常 G1 无误报。

#### M2.4 清理旧配置访问路径

- 仅在 M2.1～M2.3 全部通过后，删除无调用的重复字段和 accessor；
- 每一批删除都列出符号清单；
- **专项验收**：清理后 G1+G2。

**M2 完成条件**：业务代码不再直接使用已迁移的裸配置/状态索引，底层布局是否替换留到后续独立里程碑。

### M3：显式化 ghost 生命周期和通信契约

**目标**：把 MPI 正确性依赖从调用约定变成 API 约束。

#### M3.1 全量 callback 通信审计

- 为所有 face/corner callback 建表：Requires、Reads、Writes、Invalidates、Exchange；
- 标记 local、remote snapshot、scratch、owner commit；
- **专项验收**：每个 `p4est_iterate` 均有条目，漏传 ghost 的接口为零；仅文档/诊断改动仍须 G1。

#### M3.2 引入 `GhostSession` 兼容包装

- 包装 create/exchange/destroy/generation，旧调用暂时从包装内部转发；
- topology 变化后令旧 generation 失效；
- **专项验收**：过期 ghost 在 Debug 构建中被检测；通过 G1+G2。

#### M3.3 分阶段迁移 ghost 调用点

- 按 Gradient/AMR、balance refresh、Corner/Riemann、force/update 分批迁移；
- 每批保留旧入口，使用新入口转发；
- **专项验收**：每批 G0+相关短步，整体 G1+G2。

#### M3.4 remote 只读和 owner commit

- 引入 `RemoteCellSnapshot` 与 scratch；
- 逐 callback 消除对 ghost mirror 的权威写入；
- **专项验收**：1/2/4 ranks 改变不影响规定步数结果；通过 G1+G2+G3。

#### M3.5 通信旧路径瘦身

- 仅删除已迁移且无调用的 ghost 分配、exchange 和可写 mirror 路径；
- **专项验收**：删除前后各跑 G1，删除后再跑 G2。

### M4：AMR 子系统模块化

**目标**：AMR 决策、transfer、悬点修复可分别测试，p4est callback 仅适配数据。

#### M4.1 抽取 `AMRPolicy`

- 先把现有判据原样搬入纯接口，不改变算法；
- 建立 refine/coarsen、level、阈值和 sibling 顺序测试；
- **专项验收**：旧/新 policy 对完整测试样本逐项同判；G1+G2。

#### M4.2 抽取 `AMRTransfer`

- 抽取 parent→children 与 children→parent；旧 callback 保留为 adapter；
- **专项验收**：质量、动量、总能量、体积守恒；G1+G2。

#### M4.3 抽取 hanging repair

- 对比 balance/coarsen 两套 callback 的逐字段行为；
- 先统一到 `enforce_hanging_consistency`，确认覆盖后再删除重复实现；
- **专项验收**：幂等性、coarse-fine 几何与速度约束、G1+G2。

#### M4.4 建立 AMR controller

- 统一 refine→exchange→coarsen→balance→partition→rebuild ghost 的阶段编排；
- **专项验收**：拓扑 generation 和 exchange 次序可追踪；G1+G2+G3。

#### M4.5 AMR 旧代码瘦身

- 删除 `main.cpp` 中已被 controller/policy/transfer 覆盖的实现；
- **专项验收**：删除清单可审计，清理后 G1+G2。

### M5：Hydro/Riemann 子系统模块化

**目标**：显式分离角点贡献、主点求解、悬点约束、力组装和守恒更新。

#### M5.1 抽取纯角点数学

- 抽取矩阵组装与 2×2 求解；保留旧函数作对照；
- **专项验收**：正常、近奇异、边界条件样本逐项一致；G1。

#### M5.2 phase 化 Riemann 调用链

- 建立 reset→assemble→exchange→solve master→exchange→solve hanging→exchange→force；
- 每阶段声明输入、输出和同步边界；
- **专项验收**：阶段锚点与旧路径逐字段一致；G1+G2。

#### M5.3 明确共享角点确定性策略

- 选择 owner 求解或固定排序重复求解；
- 禁止依赖 rank、本地 `quadid` 或不稳定累加顺序；
- **专项验收**：1/2/4 ranks 的共享角点结果满足既定一致性；G1+G2+G3。

#### M5.4 抽取守恒更新与状态接受

- 分离 density、momentum、work、energy、EOS、sound speed、acceptance；
- **专项验收**：单步阶段对照、守恒量、不变量和 G1+G2。

#### M5.5 Hydro 旧代码瘦身

- 新模块完全接管后删除 `main.cpp` 中重复实现；
- **专项验收**：删除前后各 G1，删除后 G2+G3。

### M6：Mesh adapter、IO 与 Diagnostics 拆分

#### M6.1 稳定 CellKey 与 p4est adapter

- 统一 `(treeid, level, x, y)`；业务层不再直接转换 `void *`；
- **专项验收**：串并行日志和比较器能稳定对齐；G1+G2。

#### M6.2 IO 模块

- 迁移 VTK/PVTU/profile writer 与输出调度；先转发旧实现；
- **专项验收**：文件命名、字段、精度和时间元数据与旧路径一致；G1。

#### M6.3 Diagnostics 模块

- 迁移守恒监控、不变量、checksum、定点 trace；
- **专项验收**：默认关闭时无额外全场遍历和文件 IO；打开时不改变数值结果；G1+G2。

#### M6.4 清理旧 IO/诊断代码

- 确认新模块覆盖后删除旧路径和硬编码 step/坐标 trace；
- **专项验收**：清理后 G1+G2。

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
- **专项验收**：两种构建各完成 G1，最终入口再完成 G1+G2。

#### M7.4 `Simulation::run()` 编排与 `main.cpp` 瘦身

- 先新增高层编排并让 `main.cpp` 转发；
- 模块全部接管后再删除旧实现；
- **专项验收**：`main.cpp` 只保留启动；G1+G2+G3 全通过。

**M7 完成条件**：满足第 16 节 Definition of Done，并形成最终基线提交。

---

## 14. 不建议优先进行的工作

以下工作视觉收益明显，但当前不应优先：

1. 先把 `main.cpp` 按行数机械拆成多个 `.cpp`；
2. 全局一次性重命名所有变量和函数；
3. 在 ghost 语义未明确前引入复杂模板或通用 callback 框架；
4. 在 step 55 拓扑分歧未形成明确策略前声称 MPI 完全确定；
5. 同一提交同时修改算法、数据布局、目录结构和构建系统；
6. 只以“程序能运行”作为验收，不运行串行三黄金回退；
7. 为追求性能减少 ghost exchange，却没有先写清每次 exchange 的数据依赖；
8. 在新旧实现尚未完成对照和 G1 前删除旧代码；
9. 把代码瘦身与功能迁移混在同一未经验证的提交中。

---

## 15. 建议立即开始的执行队列

按依赖关系，第一轮只推进以下任务：

1. **M0.1**：核实 `validate_current.ps1` 是否真正固定并执行三黄金配置，统一摘要与退出码；
2. **M0.2**：把现有 MPI 快速测试和 4 核参考整理成明确 G2/G3 命令；
3. **M0.3**：建立子里程碑验收记录模板；
4. 完整执行一次 G1+G2+G3，形成 M0 基线提交；
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
- MPI 1/2/4 ranks 在规定步数下满足既定一致性标准；
- refine/coarsen 守恒、状态不变量和拓扑一致性均有自动测试；
- 构建系统唯一、可重复，源目录不再被构建输出污染。
