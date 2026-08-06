# M5.4 U3 最终收口记录（2026-08-07）

## 收口基线与范围

- 分支：main
- U3 基线：`e73c6e9`（U1 审计）
- M5.4 范围：U1 更新阶段审计（density/momentum/work/energy/EOS/sound speed/acceptance 已独立解耦）+ U3 守恒验证收口
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## U1 结论回顾

- 各物理量更新回调输出字段互不重叠，输入仅依赖 `m_vara`/`p4est_data`，无阶段间 ghost 耦合；
- 守恒量校验已有：`StatTotalEnergyError`、`check_state_invariants`、AMR refine 内质量/能量断言。

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 18.8 s |
| Sod AMR | 0 | 0 | PASS | 58.8 s |
| Sedov AMR | 0 | 0 | PASS | 47.0 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.3 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.1 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 29.0 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 24.4 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M5.4 结论

M5.4 守恒更新与状态接受审计完成：各物理量更新已充分分离解耦，守恒量校验保持。G0、G1、连续两次 G3 通过，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
