# DoD 闭合：构建系统唯一化（2026-08-12）

## 决策

M7.3 已证明 CMake 与 Makefile 编译同一源集合（main/alg/config_parser）与同一 C++14 标准，两套构建均成功。按 `reconstruction.md:1138` 的后续步骤「两套构建结果通过后，再决定唯一正式入口」：

- **唯一正式入口 = Makefile**（门禁实际依赖：PowerShell + `make`，产出 `bin/AMR_Solver.exe`）；
- 移除顶层 `CMakeLists.txt`（`git rm`）；
- 保留 `third_party/p4est/CMakeLists.txt`（p4est 第三方库的独立 CMake 构建，非本工程主构建）。

## 满足 DoD

- `reconstruction.md:1197`「构建系统唯一、可重复」→ 主工程仅 Makefile 一个正式入口；
- 源目录不被构建输出污染 → 构建产物入 `bin/`/`build/`，不入 Git。

## 门禁

- G0：Makefile clean build + link 验证（待执行）。
