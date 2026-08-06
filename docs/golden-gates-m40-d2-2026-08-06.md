# M4.0 D2 最终收口记录（2026-08-06）

## 收口基线与范围

- 分支：main
- D2 基线：`f3905cd`（D1）
- M4.0 代码范围：B1～B10 十项指针迁移 + D1 占位符清零
- C 系列（死计算短路）：经实测判定不适用——`edge_minmod` 的 ghost 读参与 owner `parent_gradient`，corner minmod 的 ghost 读参与 owner `SC_MAX`，短路会改变 owner 数值导致 G3 漂移；不执行。
- G2：`N/A — retired since 2026-08-04`
- 比较容差：`1e-12`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## 全量静态审计

- 生产 `src` 中 `ghost_data` 零残留（`grep -c ghost_data src/main.cpp` = 0）；
- 残留仅存在于 `src/solver/corner_solver.h` 兼容函数签名参数名（`void *ghost_data`），非解引，按计划延期；
- B 系列 10 个回调的幽灵层读取均已迁移为 `context->session->remote()`，写路径经 `context->session->data()` 保留；
- 活动 `GhostCallbackContext`、owner-local 写门禁、exchange/rebuild/generation/validity 生命周期均保留。

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
| Noh Uniform | 0 | 0 | PASS | 18.9 s |
| Sod AMR | 0 | 0 | PASS | 58.6 s |
| Sedov AMR | 0 | 0 | PASS | 46.6 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：两次完整四进程 golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

### 第一次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 31.1 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.5 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

### 第二次完整 G3

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.4 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 27.3 s |

- `mpi_gate_summary.json`：`status=PASS`，`param_restored`：`true`

## M4.0 结论

M4.0 通信旧路径剥离完成：裸 `ghost_data` 读取全部迁移为 `remote()` 只读访问，全仓占位符清零。G0、G1、连续两次 G3 全部通过，参数恢复，reference 未变化。C 系列短路以实测证据判定不适用，未执行。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
