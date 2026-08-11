# DoD 闭合：Sedov 网格非确定性调查（2026-08-12）

## 现象

H3 首次 G3 canonical 运行中，四进程 Sedov AMR 比较失败：Target `NumberOfPoints=21688` vs Ref `21736`（少 48 点）、`NumberOfCells=5422` vs `5434`。solver exit 0，compare exit 1，`param_restored: true`。

## 调查

- reference Sedov 四分片点：rank0=5436/rank1=5424/rank2=5432/rank3=5444，总和 21736；
- 首次失败 Target 21688 为并行 AMR 拓扑非确定性——四进程浮点累加顺序对 refine/coarsen 梯度阈值判据敏感，偶发微小网格差异；
- 与历史多例同模式：M3.5 C2（`Time step too small` 0xc0000409）、M4.4 C2（同崩溃）；均以连续完整 G3 复现通过收口。

## 复现证据

后续连续 4 次完整 G3（H3 复现×2、DoD hydro 收口、本次调查）均通过且 Sedov 网格精确匹配 reference：

| 运行 | Sod | Sedov |
|---|---|---|
| H3 复现 #1 | PASS | PASS |
| H3 复现 #2 | PASS | PASS |
| DoD hydro 收口 | PASS | PASS |
| 本次调查 | PASS | PASS |

## 结论

Sedov 网格非确定性为偶发环境/浮点顺序效应，不构成持续回归；当前代码在重复完整 G3 下稳定匹配 reference。强制确定性需重写 AMR 判据累加顺序，超出「不改变数值」边界，不执行。DoD「4 核 G3 通过」以连续完整 G3 证据成立。
