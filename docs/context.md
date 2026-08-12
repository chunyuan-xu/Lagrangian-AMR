# M9 重构上下文（会话恢复用）

> 本文件由 reconstruction.md §2.6 规则驱动：每个子里程碑完成后，将最新对话的核心内容写入本文件，供后续会话 resume 使用。
>
> 最近更新：2026-08-12，M9.3.1 收口后。

---

## 1. 当前项目状态

- 项目：Lagrangian-AMR（二维拉氏 AMR 求解器，p4est + MPI + C++14，MSYS2 UCRT64 + MS-MPI 构建）。
- 当前分支：`main`。
- 最近提交：`e2491af`（M9.3.1 initializer shell strip）。
- 门禁体系：G0（PowerShell 可写 TEMP 下 `make clean && make -j8` + `git diff --check`）、G1（`python/run_tests.py` 串行 Noh/Sod/Sedov golden，容差 1e-12）、G3（`python/run_mpi_gates.py` 四进程 Sod/Sedov 对 `reference/par4_*`）。G2 自 2026-08-04 起 retired。

## 2. M9 计划（docs/9.0implementation_plan.md）

M9 目标：剥离 main.cpp 剩余**包装壳函数**，压至数百行，仅留启动骨架。基线 `42d0846`（M8 收口，main.cpp 2455 行）。

| 阶段 | 内容 | 目标模块 | 状态 |
|---|---|---|---|
| M9.1.1 | AMR 标记壳（`set_*_tag`） | `src/amr/amr_callbacks.h` `AMRCallbacks` | ✅ `b78f319` |
| M9.1.2 | AMR 拓扑编排壳（`refresh_after_balance`/`append_refresh_snapshot`/`Get_AMR_BDY_info`） | `AMRCallbacks` | ✅ `69daf11` |
| M9.2.1 | Hydro 回调层 | —— | ✅ M8 已提前完成，跳过 |
| M9.2.2 | Hydro 编排壳 18 个（`predict_timestep`/`RiemannSolver`/`MatrixAssemble`/`advance` 更新族等） | `src/hydro/hydro_controller.h` `HydroController` | ✅ `6eb0890` |
| M9.3.1 | 初始化壳（`Lagrangian_init_condition`/`get_boundary_from_p4est`） | `src/init/initializer.h` `Initializer` | ✅ `e2491af` |
| M9.1.3 | AMR replace 回调（`Lagrangian_replace_quads`，父子网格数据插值） | `AMRCallbacks` | ✅ `3eae572` |
| M9.1.4 | AMR 误差估计器（`Lagrangian_refine/coarsen_err_estimate`） | `AMRCallbacks` | ✅ `待提交` |
| M9.2.3 | Hydro 流水线（`advance_single_stage`，M9.2.2 跳过项） | `HydroController` | ✅ `待提交` |
| M9.2.4 | MUSCL 角梯度回调（`quadrant_corner_minmod_estimate_callback`/`quadrant_set_gradient_zero_estimate_callback`）+ `Gradient_estimate`/`PreProcess` | `HydroCallbacks`/`HydroController` | ✅ `待提交` |
| M9.3.2 | IO 写盘（`write_solution`/`p4est_debug_output_vtu`/`debug_quadrant_copy_variable_to_array_callback`） | `IOCallbacks` | ⏳ 未开始 |
| M9.3.3 | IO 统计诊断（`write_distance_profiles`/`StatTotalEnergyError`/`StatGlobalFieldChecksum`） | `IOCallbacks` | ⏳ 未开始 |
| M9.4.1 | main.cpp 终极编排瘦身（`advance_time_step`→`Simulation`，压至数百行） | `Simulation` | ⏳ 未开始 |

## 3. 门禁记录清单（docs/golden-gates-*）

- `golden-gates-m9-1-1-2026-08-12.md`：M9.1.1
- `golden-gates-m9-1-2-2026-08-12.md`：M9.1.2
- `golden-gates-m9-2-2-2026-08-12.md`：M9.2.2
- `golden-gates-m9-3-1-2026-08-12.md`：M9.3.1
- `golden-gates-m9-1-3-2026-08-12.md`：M9.1.3
- `golden-gates-m9-1-4-2026-08-12.md`：M9.1.4
- `golden-gates-m9-2-3-2026-08-12.md`：M9.2.3
- `golden-gates-m9-2-4-2026-08-12.md`：M9.2.4

每个原子任务固定流程：G0 → G1 → G3 → reference/参数/产物检查 → focused commit → push GitHub → 门禁记录文档。任一失败停留当前项。

## 4. 环境事实（已实测）

- make 在 `C:/msys64/usr/bin/make.exe`，g++ 在 `C:/msys64/ucrt64/bin/g++.exe`（Makefile 硬编码）。
- G0 编译需在 PowerShell 中设置 `$env:PATH="C:\msys64\usr\bin;C:\msys64\ucrt64\bin;"+$env:PATH` 且设置可写 `TEMP/TMP/TMPDIR`。
- `param.ini` SHA-256 稳定为 `55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`（M9 全程未变）。
- G1 串行耗时：Noh ~18s / Sod ~58s / Sedov ~46s；G3 四进程：Sod ~30s / Sedov ~26s。
- 头文件迁移经验：从 main.cpp 迁出的头文件必须自带 `#include "hydro/hydro_callbacks.h"` 与 `"solver/hydro_callbacks.h"`（main.cpp 靠 include 顺序掩盖了缺失）；所有调用点（含 `p4est_balance` 回调）必须同步路由到新命名空间。

## 5. 剩余壳函数清单（main.cpp 当前 798 行）

| 函数 | 行数 | 归属 | 计划项 |
|---|---|---|---|
| `write_solution` | 145 | `IOCallbacks` | M9.3.2 |
| `p4est_debug_output_vtu` | 82 | `IOCallbacks` | M9.3.2 |
| `write_distance_profiles` | 23 | `IOCallbacks` | M9.3.3 |
| `StatTotalEnergyError` | 55 | `IOCallbacks` | M9.3.3 |
| `debug_quadrant_copy_variable_to_array_callback` | 40 | `IOCallbacks` | M9.3.2 |
| `advance_time_step` | 123 | `Simulation` | M9.4.1 |
| `quadtree_static` | 185 | 待识别，勿预设 | —— |

> 已迁出：`Lagrangian_replace_quads`（M9.1.3）、误差估计器（M9.1.4）、`advance_single_stage`（M9.2.3）、`StatGlobalFieldChecksum`（M9.2.3）、trace 快照（M9.2.3）、`quadrant_corner_minmod_estimate_callback`/`quadrant_set_gradient_zero_estimate_callback`/`Gradient_estimate`/`PreProcess`（M9.2.4）。main.cpp 已减至 798 行。

## 6. 下一步建议

1. **M9.3.2**：迁移 `write_solution`（145 行）+ `p4est_debug_output_vtu` + `debug_quadrant_copy_variable_to_array_callback` 到 `IOCallbacks`（`write_solution` 依赖 debug copy 回调，须同批）。
2. **M9.3.3**：迁移 `write_distance_profiles`/`StatTotalEnergyError` 到 `IOCallbacks`（`StatGlobalFieldChecksum` 已迁完）。
3. **M9.4.1**：`advance_time_step` → `Simulation`，main.cpp 压至数百行。
4. 每个原子任务完成后更新本文件第 2、3、5 节，并追加新门禁记录。

> 关键事实：`predict_timestep` 已在 M9.2.2 迁入 `hydro_controller.h`，勿重复迁移；`quadrant_corner_minmod_estimate_callback` 实测 48 行（非旧计划 184 行）；M9 全程 header-only，不新建 `.cpp`。
> M9.1.3 经验：`amr_callbacks.h` 与 `hydro_callbacks.h` 互相引用 `AMRCallbacks::`/`HydroCallbacks::`，形成 include 循环，用前置声明（`namespace HydroCallbacks { void generate_children_info_from_parent(...); }`）打破，勿直接互相 include。

## 7. 铁律提醒

- 最内层循环禁用虚函数（用 CRTP/函数对象/Lambda）；MPI 交换结构体必须保持 POD。
- 每个子里程碑：先兼容迁移，闭环后再删除瘦身；任一 G0/G1/G3 失败即停，不向后推进。
