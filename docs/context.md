# Lagrangian-AMR 重构上下文（会话恢复用）

> 由 reconstruction.md §2.6 驱动。每收口一个子里程碑更新本文件，供后续会话 resume。
> 最近更新：2026-08-13，M9 收官 + 基线 G0/G1/G3 复验 + 仓库清理完成。

---

## 1. 当前项目状态

- 项目：Lagrangian-AMR（二维拉氏 AMR 求解器，p4est + MPI + C++14，MSYS2 UCRT64 + MS-MPI）。
- 分支：`main`。最近提交：`434a685`（chore: 剔除冗余 libsc-2.8.5）。
- 提交链（M9 后）：
  - `573fe22` docs: M10 计划 + reconstruction.md §2.7 诊断产物清理规则
  - `434a685` chore: 剔除 libsc-2.8.5（251 文件，git 历史保留）
- **基线 G0/G1/G3 已复验通过**（2026-08-13，剔除 libsc 后）：G0 编译链接 PASS；G1 Noh/Sod/Sedov 全 PASS（19.5s/78.5s/61.2s）；G3 四进程 Sod/Sedov PASS（32.9s/28.2s）；`param.ini` SHA `55bccddd…` 与 M9 一致；工作区干净。**这是 M10 的可信起点锚点。**

## 2. M9 状态（已全部收口）

main.cpp 从 M8 的 2455 行减至 **346 行**（启动骨架 + `Simulation::run` 编排）。G2 全程 retired。

| 子任务 | 内容 | 提交 |
|---|---|---|
| M9.1.1/1.2 | AMR 标记壳/拓扑编排 | `b78f319`/`69daf11` |
| M9.2.2 | Hydro 编排壳 18 个 | `6eb0890` |
| M9.3.1 | 初始化壳 | `e2491af` |
| M9.1.3 | AMR replace 回调 | `3eae572` |
| M9.1.4 | AMR 误差估计器 | `854d066` |
| M9.2.3 | `advance_single_stage` | `3e0f659` |
| M9.2.4 | MUSCL 角梯度回调+壳 | `68936f9` |
| M9.3.2 | IO 写盘壳 | `a25086e` |
| M9.3.3 | IO 统计诊断壳 | `361bdda` |
| M9.4.1 | `Simulation` 编排收官 | `9787c0d` |

门禁记录：`docs/golden-gates-m9-{1-1,1-2,1-3,1-4,2-2,2-3,2-4,3-1,3-2,3-3,4-1}-*.md`（11 份）。

## 3. M10（docs/10.0implementation_plan.md）

**目标**：解构 `p4est_data_t` 上帝对象（defines.h:49 起），配置/状态/IO 读写分离。M10 全程 **header-only**（不新建 `.cpp`）。

| 子任务 | 内容 | 状态 |
|---|---|---|
| M10.1.1 | ofstream（EnergyFile/DistanceFile/ErrorFile）移出 `p4est_data_t` → `IOCallbacks` 惰性文件管理 | ✅ `待提交` |
| M10.3.1 | 运行状态迁 `SimulationClock` + 新建 `ReductionContext` | ⏳ 未开始 |
| M10.2.1 | 激活 `SimulationConfig`，分小批冻结只读配置 | ⏳ 未开始 |
| M10.4.1 | `P4estBridge` 取代 `user_pointer` | ⏳ 未开始 |

**建议顺序**：M10.1.1 → M10.3.1 → M10.2.1 → M10.4.1。

**关键事实（实测）**：`coord_type` 非只读（initializer.h:51 每步重写、hydro_callbacks.h:626 读）；`p4est_data_t` 含 config+状态+IO+枚举混合；`SimulationConfig`/`SimulationClock` 已在 `core/simulation_config.h`；M10.1.1 已删 3 个 ofstream，`p4est_data_t` 不再持非 POD 流对象。

## 4. 门禁记录清单（docs/golden-gates-*）

- M9 系列：`golden-gates-m9-{1-1,1-2,1-3,1-4,2-2,2-3,2-4,3-1,3-2,3-3,4-1}-*.md`（11 份）
- `golden-gates-m10-1-1-2026-08-13.md`：M10.1.1

每个原子任务固定流程：G0 → G1 → G3 → reference/参数/产物检查 → focused commit → push GitHub → 门禁记录文档。任一失败停留当前项。

## 4.1 环境事实（已实测）

- make：`C:/msys64/usr/bin/make.exe`；g++：`C:/msys64/ucrt64/bin/g++.exe`（Makefile 硬编码）。
- G0 命令：PowerShell 设 `$env:PATH="C:\msys64\usr\bin;C:\msys64\ucrt64\bin;"+$env:PATH` + 可写 `TEMP/TMP/TMPDIR`，`make clean && make -j8`。
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`（M9 全程未变）。
- 门禁耗时（2026-08-13 实测）：G1 Noh 19.5s / Sod 78.5s / Sedov 61.2s；G3 Sod 32.9s / Sedov 28.2s。
- 门禁入口：G1=`python/run_tests.py`（容差 1e-12）；G3=`python/run_mpi_gates.py`（四进程对 `reference/par4_*`）。
- 构建依赖链：`third_party/p4est/build/local/{include,lib}`（vendored sc，`-lp4est -lsc`）。**根目录 `libsc-2.8.5`/`p4est-2.8.5` 已删，非依赖。**

## 5. 已完成的仓库清理（2026-08-13）

- 删除未跟踪诊断产物：根目录 `*.log`、`*.plt`、`ErrorFile.txt`、`serial_golden_summary.json`/`mpi_gate_summary.json`（运行再生成）、`print_size.cpp`、`reference/output_1core.log`。
- 删除历史取证/构建目录：`.historical-g3-*`（4 个~262MB）、`.tmp/`、35 个 `build_*.o`、`step_tests/`、`build_cmake/`、根目录 `__pycache__/`。
- 从 git 剔除 `libsc-2.8.5/`（251 文件，`434a685`，保留历史）；删未跟踪 `p4est-2.8.5/`。
- **保留**：`prompt/introduction.md`（论文文稿，git 跟踪）、`reference/*.vtu`/`*.pvtu`（黄金参考）、`third_party/`、`.claude/`/`.gemini/`/`.workbuddy/`。
- 规则：reconstruction.md §2.7（大版本收口后清未跟踪诊断产物，只删未跟踪、绝不动已跟踪）。

## 6. 铁律与经验

- 最内层循环禁虚函数（CRTP/函数对象/Lambda）；MPI 交换结构体保持 POD。
- 每子里程碑：先兼容迁移→闭环→删瘦身；任一 G0/G1/G3 失败即停，不向后推进。
- 头文件迁移：迁出头文件必须自带 `hydro/hydro_callbacks.h` + `solver/hydro_callbacks.h` include；调用点同步路由。
- include 循环：`amr_callbacks.h`↔`hydro_callbacks.h` 互相引用，用前置声明打破（勿互相 include）。
- `quadtree_static`（main.cpp 内 185 行）用途待识别，未纳入 M9/M10，勿预设。
