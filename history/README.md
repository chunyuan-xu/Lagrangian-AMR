# 重构历史（history/）

本目录归档 Lagrangian-AMR 重构过程的记忆与历史记录。日常理解当前项目、编译运行、跑门禁，请以 [`docs/`](../docs/README.md) 为准；本目录仅在排查历史问题、追溯某个里程碑或门禁时查阅。

## 主线

- [`reconstruction.md`](reconstruction.md)：重构阶段划分、回退锚点、历史记录（主索引）。
- [`context.md`](context.md)：重构上下文（会话恢复用，最新进度与门禁清单）。

## 里程碑计划

- `*.implementation_plan.md`（19 份）：各里程碑（M3.5～M10）的重构计划。

## 门禁运行记录

- `golden-gates-*.md`（96 份）：每次 G0/G1/G3 门禁运行的证据记录。

## 专项审计 / 历史 bug

- [`communication_audit.md`](communication_audit.md)：M3.1 全量 callback 通信审计。
- [`MPI_BUG.SKILL.md`](MPI_BUG.SKILL.md)：Sod AMR step3 串并行分歧 bug 记录。
