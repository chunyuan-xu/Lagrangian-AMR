# M5.2 R1 门禁记录：Riemann 阶段序列抽取（2026-08-07）

## 基线与范围

- 分支：main
- R1 基线：`b30b203`（M5.2 计划）
- 生产改动：
  - 新建 `src/solver/riemann_phases.h`：`RiemannPhases::run_iteration`，显式声明 assemble→exchange→solve master→exchange→solve hanging→exchange 阶段链；
  - `src/main.cpp`：`RiemannSolver` 迭代体替换为对该接口的调用；添加 include。
- 未修改：阶段顺序、`fixed_iter_num` 迭代次数、exchange 边界、各阶段数值。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- PowerShell 同一进程设置可写 `TEMP/TMP/TMPDIR`
- `make clean`：PASS
- `make -j8`：PASS
- 链接生成 `bin/AMR_Solver.exe`：PASS
- `git diff --check`：PASS

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.4 s |
| Sod AMR | 0 | 0 | PASS | 59.2 s |
| Sedov AMR | 0 | 0 | PASS | 46.6 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.4 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## R1 结论

R1 的 G0、G1、G3 全部通过。Riemann 阶段链已显式 phase 化并抽取为 `RiemannPhases`，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
