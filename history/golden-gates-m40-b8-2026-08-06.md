# M4.0 B8 门禁记录：get-children-hanging-info 指针迁移（2026-08-06）

## 基线与范围

- 分支：main
- B8 基线：`d7c742a`（B7）
- 生产改动：仅 `src/main.cpp`
- 迁移回调：`quadrant_get_children_hanging_info_callback`
- 修改内容：
  - 回调体两处 `ghost_data[...]` 解引 → `context->session->remote()`（只读 `m_cndata`）；
  - 删除 `ghost_data` 声明；
  - 注册：`Get_AMR_BDY_info` 中该回调的 user_data 由 `(void*)session.data()` 改为复用已声明的 `&callback_context`。
- 修正：首次 G0 因 `Get_AMR_BDY_info` 内重复声明 `callback_context` 失败，修正为复用 B7 已声明实例后通过。
- 未修改：children hanging 信息组装、owner 写入门禁（`if (!is_ghost)`）、exchange 时序、ghost 生命周期。
- G2：`N/A — retired since 2026-08-04`
- reference：未修改或重新生成
- `param.ini` SHA-256：`55bccddd799b613c2a2ec7d7f31380f66ed7af09ec0365e087d30a78ac0b9a94`

## G0

- 首次尝试因重复声明失败，不作为有效门禁。
- 修正后有效 G0：PowerShell 同进程可写 `TEMP/TMP/TMPDIR`；`make clean`、`make -j8`、链接均 PASS；`git diff --check` PASS。

## G1：serial golden rollback

入口：`python/run_tests.py`；固定顺序：Noh Uniform → Sod AMR → Sedov AMR；容差：`1e-12`。

| 算例 | solver | compare | 状态 | 用时 |
|---|---:|---:|---|---:|
| Noh Uniform | 0 | 0 | PASS | 19.2 s |
| Sod AMR | 0 | 0 | PASS | 59.6 s |
| Sedov AMR | 0 | 0 | PASS | 47.0 s |

- `serial_golden_summary.json`：`status=PASS`
- `param_restored`：`true`
- 三个算例均实际执行

## G3：四进程 MPI golden rollback

入口：`python/run_mpi_gates.py`；固定顺序：Sod AMR → Sedov AMR；四进程；比较 `reference/par4_*`。

| 算例 | ranks | solver | compare | 状态 | 用时 |
|---|---:|---:|---:|---|---:|
| Sod AMR | 4 | 0 | 0 | PASS | 30.7 s |
| Sedov AMR | 4 | 0 | 0 | PASS | 25.3 s |

- `mpi_gate_summary.json`：`status=PASS`
- `param_restored`：`true`
- 两个算例均实际执行

## B8 结论

B8 的有效 G0、G1、G3 全部通过。get-children-hanging-info 回调的幽灵层读取已迁移为 `remote()` 只读访问，参数恢复，reference 未变化。未将 `.o`、输出目录、VTU/PVTU、summary JSON 或调试产物加入提交。
