# M4.3 H1 字段行为对比审计结论（2026-08-07）

## 对比对象

- `quadrant_update_after_balance_callback`（main.cpp:1623，balance 后）
- `quadrant_set_init_parent_edge_callback`（main.cpp:3294，coarsen 前）
- `quadrant_reset_hanging_info_callback`（main.cpp:4385，清零）

## 逐字段写入清单

### after_balance（owner child 约束）

| 字段 | 门禁 |
|---|---|
| `child_vara->corner_vector(idcnCoords_cur/lag, corner)` | `!is.hanging.is_ghost[0/1]` |
| `child_vara->corner_vector(idcnVelocity_cur/lag, corner)` | 同上 |
| `child_vara->cell(idVolume)` | 同上 |
| `child_vara->cell(idDensity_cur)` | 同上 |
| `child_vara->cell(idPressure_cur)` | 同上 |

### set_init_parent_edge（parent-edge 组装）

| 字段 | 门禁 |
|---|---|
| `PCInfo[parent_face_index].IsParentChildBoun` | `!is.full.is_ghost` |
| `PCInfo[parent_face_index].ParentPIStar` | 同上 |
| `PCInfo[parent_face_index].Hanging_velocity` | 同上 |
| `PCInfo[parent_face_index].Lcp[0/1]` | 同上 |
| `PCInfo[parent_face_index].Ncp[0/1]`（预减） | 同上 |
| `m_plus->Lcp` / `m_minus->Lcp`（parent cndata 半边区） | 同上 |

## 对比结论

- 两回调**写入字段完全不相交**：一个写 child 角落坐标/速度/体积/密度/压力，另一个写 parent 的 PCInfo 与半边区 Lcp；
- 共享点仅「按 `parent_face_index` 取 master 边两角坐标」的逻辑片段，但语义不同（after_balance 用 master 中点做约束，set_init_parent_edge 用 master 距离算 Lcp）；
- `quadrant_reset_hanging_info_callback` 只清零标志，不参与约束。

**判定**：两回调不是重复实现，不存在可安全合并的共享约束核心。强制统一为 `enforce_hanging_consistency` 会引入无收益的重构风险，M4.3 不执行 H2 合并，以审计结论收口。

## 幂等验证

- `refresh_after_balance` 的幂等断言（main.cpp:5068 `refresh_after_balance is not idempotent`）为既有运行时检查；
- 通过 H3 的 G1/G3 实证 after_balance 重复调用不改变输出。

## H3 收口门禁

- G0：clean build PASS
- G1：Noh/Sod/Sedov 三项 PASS
- G3：四进程 Sod/Sedov 两次完整运行 PASS
- `param_restored`：true；reference 未变化
