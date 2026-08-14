# M6 M6.4 旧 IO/诊断代码清理审计（2026-08-07）

## 审计范围

确认新模块覆盖后，main.cpp 中旧 IO/诊断代码的可删除项。

## 现状与结论

- **零引用死函数**：`zero-use: NONE`——main.cpp 无被 M6 覆盖后可删除的死代码；
- **Diagnostics**：M6.3 已将 p4est adapter（`check_state_invariants`/`invariant_volume_callback`/`InvariantContext`）迁移到 `src/diagnostics/state_invariant_checker.h`，main.cpp 无本地残留；
- **IO**：生产 `write_solution` 因依赖静态 `quadrant_copy_variable_to_array_callback` 与 `#ifdef P4_TO_P8` 分支，迁移延期（M6.2 审计记录），`write_solution` 保留在 main.cpp；`src/io/vtk_writer.h` 提供封装；
- **硬编码 step trace**：`current_step == 3`/`== 1` 等均被 `target_trace_enabled()` 门控（默认关闭），符合「默认关闭时无额外 IO；打开时不改变数值」；
- **守恒监控**：`StatTotalEnergyError`（含 `current_step == 1` 初始能量记录）为活动守恒功能，保留。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.7 s |
| Sod AMR | 0 | 0 | PASS | 58.7 s |
| Sedov AMR | 0 | 0 | PASS | 45.3 s |

- `serial_golden_summary.json`：`status=PASS`，`param_restored`：`true`
- 三个算例均实际执行

## 结论

M6.4 无死代码可清理，以审计 + G1 收口。参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
