# M3.1 全量 callback 通信审计（Communication Audit）

> 里程碑：M3.1 全量 callback 通信审计。审计对象为 `src/main.cpp` 中全部
> `p4est_iterate` 调用与其注册的 callback。目标：为每个 face/corner callback
> 建立通信契约（Requires / Reads / Writes / Invalidates / Exchange），核实
> 是否存在漏传 ghost 的接口，为 M3.2 引入 `GhostSession` 提供依据。
>
> 审计日期：2026-08-04。基线提交：`c40e2f2`（M2.4）。

## 0. 审计方法

- 逐一定位全部 51 个 `p4est_iterate` 调用（2D 下签名
  `(p4est, ghost, user_data, volume_cb, face_cb, corner_cb)`；
  `#ifdef P4_TO_P8` 内的 `NULL` 为 3D 专用参数）。
- 按 callback 定义签名（`p4est_iter_volume/face/corner_info_t`）分类。
- 对每个 face/corner callback 阅读函数体，记录其通过 `info->quad->p.user_data`
  （local）与 `ghost_data[quadid]`（remote）访问的字段、写入位置、被下游消费
  的字段，以及所在 `advance_time_step` / `advance_single_stage` / `RiemannSolver`
  中的 ghost 交换时序。

## 1. 结论摘要

1. **漏传 ghost 的活跃接口为零。** 全部 12 个活跃 face/corner callback 均以
   非空 `ghost` 调用并传入 `ghost_data` 作为 `user_data`。唯一以 `NULL ghost`
   注册的 face callback（`quadrant_update_after_coarsening_callback`）位于
   **从未被调用**的 `postprocess_after_coarsening` 内，属死代码（若执行会因
   `ghost_data` 解引用崩溃）。
2. **发现 4 个死符号**（见 §4）：`postprocess_after_coarsening`、
   `quadrant_update_after_coarsening_callback`、`quadrant_update_parent_velo_press_callback`、
   `quadrant_vtk_coord_update_callback`。均定义于 `main.cpp` 但仓库内零调用。
3. **跨 rank 隐患（M3.4 目标）**：多个 face/corner callback 通过可写
   `quad_data_t*` 修改可能指向 `ghost_data` 的单元（如
   `m_child1_data = &ghost_data[quadid]`）。`p4est_ghost_exchange_data` 只做
   owner→ghost 单向拷贝，任何对 ghost mirror 的写会静默丢失；只有对 owner
   单元的写会在下一次 exchange 时发布。这与 §5.2「禁止把 ghost mirror 当成
   权威状态写入」一致，是 M3.4 remote 只读化的直接依据。

## 2. face/corner callback 通信契约

| # | callback | 类型 | 行 | 调用点 | ghost | 核心 Reads | 核心 Writes | Exchange 供给 |
|---|---|---|---|---|---|---|---|---|
| 1 | `quadrant_relaxed_hanging_solver_callback` | face | 398 | 3133 | ✅ | child/parent `idcnVelocity_lag`、child `idMass`/`idTotalEnergy_cur`、child `points[].MatrixP/RHS` | child hanging 角 `idcnVelocity_lag`、`idcnFluxRelaxed`、parent `m_pc_edge_data` | L3118 前 / L4115 后 |
| 2 | `quadrant_edge_minmod_estimate_callback` | face | 1509 | 5159 | ✅ | parent+child `cell(idCPara)`、`cell_vector(idCentroidCoord_cur)` | `edge(idEPara, face)`（child/parent/brother） | L5618/L5722（PreProcess） |
| 3 | `quadrant_update_after_coarsening_callback` | face | 1711 | 5130 | ❌ NULL | —（死代码，未执行） | — | 无 |
| 4 | `quadrant_whether_allowing_coarsening_from_edge_callback` | face | 1884 | 4963 | ✅ | child/parent `level` | `int_cell(idAllowCoarsening)` | L5659 |
| 5 | `quadrant_update_after_balance_callback` | face | 1943 | 5103 | ✅ | parent master `idcnCoords_cur`/`idcnVelocity_lag`、child `idcnCoords_cur`/`idcnVelocity_cur` | child `idcnCoords_cur/lag`、`idcnVelocity_cur/lag`、`cell(idVolume)`、`cell(idDensity_cur)`、`cell(idPressure_cur)` | L5702 前 / L5722 后 |
| 6 | `quadrant_hanging_point_matrix_assemble_callback` | face | 2789 | 3121 | ✅ | parent `idcnCoords_relaxed`、child `MarCnData[idcnMcp]`+`idcnRHS`、parent `MarCnData[ideMcp]`+`ideRHS`、hdata | child `points[].MatrixP/RHS`、`.IsHanging`、`.TwoBouns`、`.master_coord_relaxed`、`.hanging_coord` | L3118 前 / L4115 后 |
| 7 | `quadrant_set_init_parent_edge_callback` | face | 3668 | 3921 | ✅ | child `points[].IsHanging`/`.pi_constrained_parent`、child+parent `idcnVelocity_lag`/`idcnCoords_cur`、hdata | parent `m_pc_edge_data`（`IsParentChildBoun`/`ParentPIStar`/`Hanging_velocity`/`Lcp`/`Ncp`）、parent `cndata` Lcp | L4179 前 / L4184 后 |
| 8 | `quadrant_get_children_hanging_info_callback` | face | 3812 | 3897 | ✅ | child hdata `Ncp`/`Lcp`/`delta_u_cp`/`Uc_cur`/`Zcp` | child `points[].IsHanging`、`.TwoBouns` | L4179 前 / L4184 后 |
| 9 | `quadrant_corner_minmod_estimate_callback` | corner | 1379 | 5170 | ✅ | 角周边全部 quad 的 `cell(idCPara)`、`cell_vector(idCentroidCoord_cur)` | `corner(idCNPara, cnid)`（local+ghost side） | L5618/L5722 |
| 10 | `quadrant_whether_allowing_coarsening_from_corner_callback` | corner | 1458 | 4974 | ✅ | side quad `level` | `int_cell(idAllowCoarsening)` | L5659 |
| 11 | `quadrant_corner_to_point_matrix_assemble_callback` | corner | 2394 | 2982 | ✅ | 各 side quad `MarCnData[idcnMcp]`+`idcnRHS`、hdata `enumBYD`/`BYDVal` | 各 side `points[].MatrixP/RHS`、边界 `points[].TwoBouns` | L2979 前 / L4100 后 |
| 12 | `quadrant_corner_velocity_callback` | corner | 3146 | 3254 | ✅ | `points[].MatrixP/RHS/TwoBouns`、hdata `enumBYD` | `points[].velo_lag`、各 side `corner_vector(idcnVelocity_lag, cnid)` | L4100 前 / L4105 后 |
| 13 | `quadrant_update_parent_velo_press_callback` | face | 3005 | 无 | — | —（死代码，未注册） | — | 无 |

> 补充说明：
> - #7 回调体内部把同一 `user_data` 同时强转为 `p4est_data_t*` 与 `quad_data_t*`
>   （L3671-3672），其中 `p4est_data` 声明后从未使用，误转 inert，不影响正确性，
>   但应作为类型安全债务在 M3.2/M5.x 消除。
> - #8 读取的 `delta_u_cp`/`Uc_cur`/`Zcp` 此刻尚未在本步刷新（它们在
>   `RiemannSolver` 内由 `quadrant_corner_matrix_assemble_callback` 写入），
>   因此拷贝的是上一步残留值——潜在 stale 隐患，建议 M3.4 一并确认。
> - #5/#1/#11/#12 对 ghost copy 的写入会被下一次 owner→ghost exchange 覆盖，
>   不会传播；仅 owner 本地写入生效（见 §1.3）。

## 3. ghost 生命周期与交换时序

`advance_time_step`（主循环）内的 ghost 生命周期：

| 行 | 操作 | 供给的 phase |
|---|---|---|
| L5614 | `p4est_ghost_new(P4EST_CONNECT_FULL)` | 初始 |
| L5618 | `p4est_ghost_exchange_data` | 首步 PreProcess（callback 2/9） |
| L5655/5659 | `p4est_refine_ext` 后重建 + exchange | `set_allowing_coarsening_tag`（callback 4/10）→ `p4est_coarsen_ext` |
| L5676-5679 | `p4est_coarsen_ext`+`p4est_balance_ext` 后 destroy | — |
| L5684-5695 | `p4est_partition` 后 destroy | — |
| L5700/5702 | 重建 + exchange | `refresh_after_balance`（callback 5） |
| L5722 | 步末 exchange | 下一步 PreProcess + 本步 `advance_single_stage` |

`advance_single_stage` / `RiemannSolver` 内的交换：

| 行 | 操作 | 供给的 phase |
|---|---|---|
| L4179 | exchange | `Get_AMR_BDY_info`（callback 8/7） |
| L4184 | exchange | `RiemannSolver` |
| L2979 | `MatrixAssemble` 内部 exchange | volume→corner assemble（callback 11） |
| L4100 | exchange | `ComputeCornerNodeVelocity`（callback 12） |
| L4105 | exchange | `ComputeHangingNodeVelocity...`（callback 6/1） |
| L3118 | `ComputeHangingNode...` 内部 exchange | `hanging_point_matrix_assemble`（callback 6） |
| L4115 | Riemann 迭代末 exchange | — |

结论：每次需要跨 rank 邻居数据的 face/corner phase 前都有一次 `ghost_exchange`；
拓扑变化（refine/coarsen/balance/partition）后 ghost 均被重建再交换。时序与
§2 表中各 callback 的 Exchange 列一致。

## 4. 死代码清单

| 符号 | 定义行 | 证据 |
|---|---|---|
| `postprocess_after_coarsening` | 5128 | 仓库内仅有定义与内部 callback 引用，无任何调用 |
| `quadrant_update_after_coarsening_callback` | 1711 | 仅注册于上述未调用函数；且以 `ghost=NULL, ghost_data=NULL` 注册但函数体解引用 `ghost_data` |
| `quadrant_update_parent_velo_press_callback` | 3005 | 从未注册进任何 `p4est_iterate` |
| `quadrant_vtk_coord_update_callback` | 791 | 从未注册进任何 `p4est_iterate` |

> 处理建议：按 M2.4 同一纪律，在 M3.5 通信旧路径瘦身或独立清理里程碑中删除
> 这些零调用符号；删除前各自跑 G1。

## 5. 与后续里程碑的衔接

- **M3.2（GhostSession）**：§3 的 ghost 生命周期可被封装为
  `GhostSession::build / exchange / invalidate_after_topology_change`；
  §2 表中每个 callback 的 Requires 即 GhostSession 的 generation 约束。
- **M3.3（分阶段迁移）**：按「Gradient/AMR（callback 2/4/9/10）→ balance
  refresh（callback 5）→ Corner/Riemann（callback 11/12/6/1）→ Get_AMR_BDY_info
  （callback 7/8）」分批切换。
- **M3.4（remote 只读 + owner commit）**：§1.3 的 ghost-write 隐患即本次改造
  目标；callback 1/5/7/8/9/11/12 需区分 local 权威写与 remote snapshot 只读。
