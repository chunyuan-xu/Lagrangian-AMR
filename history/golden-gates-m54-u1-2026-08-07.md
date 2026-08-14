# M5.4 U1 更新阶段审计（2026-08-07）

## 阶段清单与独立性

| 阶段 | 回调 | 输入 | 输出 | 同步 |
|---|---|---|---|---|
| Density | `quadrant_update_density_callback`（2689） | coords_lag | Volume, Density_lag | 无 |
| Momentum | `quadrant_update_momentum_callback`（2721） | Fcp/FluxRelaxed/Mass | CentroidVelo_lag | 无 |
| Work | `quadrant_compute_work_callback`（2788） | velo/force | KineticVariation, TotalWork | 无 |
| Energy | `quadrant_update_energy_callback`（2862） | TotalEnergy_half/Work | TotalEnergy_lag, InternalEnergy_lag | 无 |
| EOS | `quadrant_update_EOS_callback`（2922） | Gamma/Density_lag/IE_lag | Pressure_lag | 无 |
| SoundSpeed | `quadrant_compute_soundspeed_callback` | Gamma/Pressure/Density | SoundSpeed | 无 |
| Accept | `quadrant_accept_center_solution_callback`（2953） | *_lag | *_cur 全量接受 + children info | 无 |

## 独立性结论

- 各回调输入仅依赖 `m_vara`/`p4est_data`，输出字段互不重叠；
- 在 `advance_single_stage` 中按序调用，无阶段间 ghost 依赖（ghost 同步仅发生在 Riemann 迭代内部与阶段间 exchange）；
- 各物理量更新已充分解耦，可在未来独立单元测试。

## 守恒量校验

- `quadrant_total_energy_error_callback`（2985）累加 total_energy_lag/cur；
- `StatTotalEnergyError` 与 `check_state_invariants` 提供运行时质量/能量/动量/体积守恒检查；
- AMR refine 内另有质量/能量守恒断言（1e-10）。

## 结论

M5.4 守恒更新与状态接受已充分分离，无需代码修改。以审计 + 守恒验证收口。
