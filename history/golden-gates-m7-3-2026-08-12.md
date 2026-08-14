# M7.3 构建系统统一（2026-08-12）

## 基线与范围

- 分支：main
- M7.3 基线：`c8d7dc5`（M7.2）
- 生产改动：`CMakeLists.txt`：
  - `CMAKE_CXX_STANDARD` 从 11 升到 14（与 Makefile `-std=c++14` 一致）；
  - 源文件集合补 `src/io/config_parser.cpp`（与 Makefile 的 `main.cpp`+`alg.cpp`+`config_parser.cpp` 一致）。
- 未修改：Makefile、数值公式、容差。

## 构建验证

- **CMake**：`cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release` 配置成功；`cmake --build build-cmake --target AMR_Solver -j 8` 编译 main/alg/config_parser 并链接生成 `bin/AMR_Solver.exe`（exit 0）。
- **Makefile**：由 M7.3 后门禁 G0/G1/G3 验证（同一源集合 + C++14）。
- `build-cmake/` 临时目录已清理，未入 Git。

## 结论

两套构建现在编译同一源文件集合（main/alg/config_parser）与同一 C++ 标准（C++14），无冲突。M7.3 以构建一致性收口。
