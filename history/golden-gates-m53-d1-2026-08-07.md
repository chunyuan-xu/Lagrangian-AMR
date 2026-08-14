# M5.3 D1 共享角点确定性策略审计（2026-08-07）

## 策略结论

共享角点采用 **owner 求解策略**：

1. **组装**（`quadrant_corner_to_point_matrix_assemble_callback`，main.cpp:2017）：对每个共享角点，遍历 p4est corner iterate 的 `sides`，累加各 cell 的 `MarCnData[idcnMcp]` 与 `idcnRHS` 到 MatrixP/RHS；owner（`!is_ghost`）写回 `points[cnid].MatrixP/RHS`。
2. **求解**（`quadrant_corner_velocity_callback`，main.cpp:2487）：owner 调用 `CornerSolve::boundary_node_velocity` 求解共享角点速度并写 `velo_lag`；ghost 侧只读。
3. **边界检测**：第一遍循环读各 side 的 `enumBYD` 判定 `is_boundary`，第二遍循环 owner 求解。

## 确定性证明

- **不依赖 rank**：`is_ghost` 由 p4est corner iterate 提供，owner 判定是拓扑确定的；每个共享角点恰好被一个 owner 求解。
- **不依赖本地 quadid**：累加顺序完全由 `sides` 数组顺序决定，p4est 的 corner iterate 保证确定性遍历（跨 rank 一致的拓扑序）。
- **无不稳定累加**：`MatrixP += ...` / `RHS += ...` 的浮点累加顺序确定，不随进程或迭代漂移。
- **不依赖本地 quadid 数值**：`quadid` 仅用于索引 ghost/owner 数据，不参与累加顺序或数值。

## 一致性验证依据

- G1（1 rank serial）reference：`Noh_32x32.vtu`、`SodAMR.vtu`、`SedovAMR.vtu`；
- G3（4 ranks）reference：`par4_sod`、`par4_sedov`；
- G1/G3 对同一算例通过 `1e-12` 容差比较，实证共享角点结果跨 rank 一致。

## 结论

M5.3 确定性策略为 owner 求解，满足「禁止依赖 rank、本地 quadid、不稳定累加顺序」的约束。无需代码修改。
