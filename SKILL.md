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

## 五大智能体 SOP

### 1. 裁判验证智能体 (Referee Validation Agent)

**角色**: 严格裁判，只判决，不修复。

**调用时机**: 任何代码修改后，需要官方验证并提交时。

**完整 SOP**:
1. 申请权限：`ask_permission` 授权 `make`、`python`、`git`、`powershell`
2. 运行：`powershell -File .\validate_current.ps1`
3. **若通过（Exit Code 0）**:
   - `git add .`
   - `git commit -m "chore: [GOLDEN-PASS] pass validation regression suite"`
   - `git push origin main`
   - 报告 PASS + commit hash
4. **若失败（Exit Code 1）**:
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

**调试重点领域**:
- Ghost 层数据失效（p4est_refine/coarsen 后未重建 ghost）
- 迭代回调中 NULL ghost 解引用
- 分区边界的节点力不对称
- 全局量 Allreduce（能量、时间步）
- partition 后的数据迁移（`p4est_partition` 时序）

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
- 🔄 AMR 并行（Sedov AMR）：BUG 1-5 已修复，BUG 7（分区边界节点力）调查中

### 主要代码位置
- 时间推进主循环: `main.cpp: advance_time_step()` (~L5160)
- AMR 回调（refine/coarsen/balance replace）: `main.cpp: Lagrangian_replace_quads()` (~L4130)
- 节点速度求解: `main.cpp: RiemannSolver()` (~L3891)
- 能量守恒检查: `main.cpp: StatTotalEnergyError()` (~L3752)
- Ghost 层刷新: `main.cpp: refresh_after_balance()` (~L4734)
