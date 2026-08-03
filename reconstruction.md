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
| `quick_consistency_test.py` | 指定步数串行/MPI 快速一致性测试 | 可作为重构期间的核心回归门禁 |
| `run_tests.py` / `compare_vtu.py` | 三个串行锚点回归及 VTU 比较 | 需固定全部锚点参数、Python 环境、参数恢复和安全输出轮转 |

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

### 2.2 每次提交的边界

每个重构提交应满足：

- 只处理一个概念，如“封装 ghost 生命周期”或“抽取 AMR criterion”；
- 不同时进行算法修复和大规模重命名；
- 先保证串行锚点，再保证 MPI 一致性；
- 比较单元时使用 `(treeid, level, x, y)`，不依赖 `quadid`；
- 若网格数量或稳定几何集合不同，立即停止字段比较并定位拓扑首分歧；
- 任何 `p4est_iterate` face/corner callback 都必须显式说明是否需要 ghost、读哪些字段、写哪些字段、何时 exchange。

---

## 3. P0：重构前的正确性冻结与风险修复

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
3. 正式改造 `run_tests.py`：
   - 每个算例显式设置 `refine_err`、`coarsen_error` 等完整配置；
   - `finally` 恢复 `param.ini`；
   - 固定含 NumPy 的 Python 解释器或取消不必要依赖；
   - 轮转输出目录，不使用递归删除；
   - 生成机器可读摘要。
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

## 13. 推荐实施里程碑

### 里程碑 M0：基线可信

- 三个串行锚点全部可重复；
- MPI step 3/4/10/50/54 门禁固定；
- 回归配置完全显式；
- 当前 step 55 问题被独立跟踪。

### 里程碑 M1：P0 正确性问题清零

- timestep 局部最小值正确；
- coarsen family 判据明确；
- solver/coord 枚举使用正确；
- RK 和输出时间语义明确；
- 每项修改都有独立回归。

### 里程碑 M2：状态与通信可解释

- typed config 和 typed state accessor 上线；
- `GhostSession` 管理生命周期；
- remote ghost 默认只读；
- callback context 类型化；
- 每个 phase 有读写/同步契约。

### 里程碑 M3：AMR 可独立测试

- policy、transfer、hanging repair 成为纯模块；
- p4est adapter 只做数据映射；
- refine/coarsen 守恒和顺序无关测试通过。

### 里程碑 M4：Hydro 可独立测试

- Riemann、角点速度、悬点约束、force 和状态更新分模块；
- 共享角点 owner/确定性策略明确；
- MPI 分区改变不影响算法结果。

### 里程碑 M5：主程序瘦身

- IO、diagnostics、mesh adapter 完成拆分；
- `main.cpp` 只保留启动和 `simulation.run()`；
- 所有历史锚点、MPI 门禁和长期回归通过。

---

## 14. 不建议优先进行的工作

以下工作视觉收益明显，但当前不应优先：

1. 先把 `main.cpp` 按行数机械拆成多个 `.cpp`；
2. 全局一次性重命名所有变量和函数；
3. 在 ghost 语义未明确前引入复杂模板或通用 callback 框架；
4. 在 step 55 拓扑分歧未处理前声称 MPI 完全确定；
5. 同一提交同时修改算法、数据布局、目录结构和构建系统；
6. 只以“程序能运行”作为验收，不比较稳定网格键、字段和守恒量；
7. 为追求性能减少 ghost exchange，却没有先写清每次 exchange 的数据依赖。

---

## 15. 第一批建议落地任务（按执行顺序）

1. 完成并固化 Noh/Sod/Sedov 回归工具；
2. 为 timestep 局部最小值编写最小测试并修复；
3. 为 coarsen sibling policy 编写纯函数测试，确认语义后修复；
4. 核对 Riemann 门控设计，消除 `coord_type` 与 `RiemannSolver` 的跨枚举比较，并明确 RK 阶段；
5. 新增 `CellKey`、`SimulationConfig`、`SimulationClock`；
6. 给旧 `CVariable` 添加 typed accessor 与不变量检查；
7. 引入 `GhostSession`，只替换生命周期管理，不立即改数值算法；
8. 为所有 face/corner callback 建立 ghost 审计清单；
9. 先抽取 `AMRPolicy`，再抽取 `AMRTransfer`；
10. 最后抽取 Riemann/Corner/Hanging solver，并逐步缩减 `main.cpp`。

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
- Noh、Sod、Sedov 串行锚点通过；
- MPI 1/2/4 ranks 在规定步数下满足既定一致性标准；
- refine/coarsen 守恒、状态不变量和拓扑一致性均有自动测试；
- 构建系统唯一、可重复，源目录不再被构建输出污染。
