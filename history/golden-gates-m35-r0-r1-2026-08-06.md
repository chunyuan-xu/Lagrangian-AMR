# M3.5 R0/R1 门禁记录（2026-08-06）

## 基线

- 分支：`m35-communication-cleanup`
- 基线提交：`bed5156`（main B15 收口及 GCC 环境复盘）
- G2：`N/A — retired since 2026-08-04`
- `param.ini` 运行前 SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`
- reference 文件数：46
- reference 集合 SHA-256：`5b8a23375293300a8300a99fa1e646443e9cf9f7c556ffd0ff2bc53901c1f2e7`
- Python：3.14.3
- NumPy：2.4.3
- 构建环境：Windows PowerShell 同一进程设置项目下可写 `TEMP/TMP/TMPDIR`；MSYS2 UCRT64 `g++.exe`；MS-MPI；p4est 2.8.5

## R0：入口基线复核

R0 不修改生产源码、runner、`param.ini` 或 reference。使用正式 Makefile 和 canonical runners。

### G0

- `make clean`：PASS
- `make -j8`：PASS
- `bin/AMR_Solver.exe`：链接生成
- 仅有既有 `-Wall` warnings，无编译/链接错误

### G1

入口：`python/run_tests.py`

固定顺序：Noh Uniform → Sod AMR → Sedov AMR

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.0 s |
| Sod AMR | 0 | 0 | PASS | 57.9 s |
| Sedov AMR | 0 | 0 | PASS | 46.1 s |

- tolerance：`1e-12`
- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行，未执行项无 PASS 冒充

### G3

入口：`python/run_mpi_gates.py`

固定四进程顺序：Sod AMR → Sedov AMR

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.1 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 26.8 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行
- G2：N/A

### R0 结论

R0 完整通过 G0/G1/G3，可以进入 R1 清理契约审计。

## R1：零调用清理契约

R1 只做静态审计，不修改生产源码。

| 候选组 | 当前证据 | 阶段结论 |
|---|---|---|
| `GhostSession::data_size_` | 仅在构造、initialize、destroy 和私有声明中出现；无读取、容量检查、exchange、snapshot、generation 或 validity 消费 | 可进入 C1 |
| `quadrant_vtk_coord_update_callback` | 仅有静态定义；无调用、注册、函数指针或测试引用 | 可进入 C2 |
| `Predict_refining_Quads` + `quadrant_predict_refining_quads_callback` | wrapper 无调用；callback 只在该不可达 wrapper 内注册 | 作为原子组进入 C3 |
| `postprocess_after_coarsening` + `quadrant_update_after_coarsening_callback` | wrapper 无调用；callback 只在该不可达 wrapper 内注册 | 作为原子组进入 C4 |
| `quadrant_update_parent_velo_press_callback` | 仅有静态定义；无调用、注册、函数指针或测试引用 | 可进入 C5 |

### 必须保留的活动路径

- `GhostSession::get()`、`data()`、`remote()`、`valid_remote_id()`；
- `GhostSession::initialize()`、`destroy()`、`rebuild()`、`exchange()`；
- generation、topology version 和 validity assert；
- `GhostCallbackContext` 及活动 callback context；
- owner-local 写入门禁和 remote const 读取；
- 活动 `p4est_iterate` 注册、ghost/context 参数和交换时序；
- `corner_solver.h` 兼容声明以及全部活动物理、AMR、hanging 路径。

### 延期项

R1 不删除活动 GhostSession API，不迁移 raw context，不收紧活动 typed callback，不修改 ghost allocation/exchange/rebuild/generation，不修改 AMR、hanging repair、Riemann、corner matrix、velocity 或 gradient。

## R1 结论

R1 清理契约和五组零调用证明完成。下一阶段只能进入 C1，且必须先对 C1 做 focused diff，再执行完整 G0/G1/G3。任何门禁失败都停留在当前阶段。
