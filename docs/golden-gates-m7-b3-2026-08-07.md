# M7 B3 最终主程序审计（2026-08-07）

## 审计范围

核对 `main()` 结构与模块分工，确认无残留旧 IO/诊断。

## 现状

`main()`（main.cpp:4615）结构：

1. **初始化**：`sc_MPI_Init` → `sc_init` → `p4est_init`；
2. **配置**：`IOAlgorithm::ConfigParser("param.ini")` → `ctx.load_from_config` → 有效性检查；
3. **网格**：`p4est_connectivity_new_unitsquare` → `p4est_new_ext`（单位正方形，最小层级来自 startup_config）；
4. **时间推进**：`advance_time_step`（4673），内部含：
   - `PreProcess`
   - `AMRController::execute_amr` / `execute_partition`
   - `RiemannSolver`（RiemannPhases）
   - `write_solution`（IOAlgorithm::write_solution_file）
   - `StatTotalEnergyError`
   - `AcceptNumericalSolution`
   - `Diagnostics::check_state_invariants`
5. **收尾**：`ghost_session.destroy`、MPI 结束。

## 模块分工

- 基础数学：`core/vector_matrix.h`（Vec2/Mat2 别名）
- AMR：`amr/`（criteria/transfer/controller）
- Mesh：`mesh/`（GhostSession、cell_key）
- Solver：`solver/`（CornerSolve、RiemannPhases、corner_solver、solver_gate）
- Physics：`physics/`（eos、corner_solve、stage_policy、timestep_reduction）
- IO：`io/`（vtk_writer、output_stamp、config_parser、solution_writer 审计记录）
- Diagnostics：`diagnostics/`（state_invariant_checker）

## 结论

主程序结构清晰，模块分工完整，无残留旧 IO/诊断死代码。B3 以静态审计收口，无生产代码改动。
