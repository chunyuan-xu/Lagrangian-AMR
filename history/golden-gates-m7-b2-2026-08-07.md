# M7 B2 构建一致性审计（2026-08-07）

## 审计范围

核对 Makefile 与 p4est CMake 编译同一源文件集合与 C++ 标准。

## 现状

- **Makefile**：
  - 编译器：`C:/msys64/ucrt64/bin/g++.exe`（MSYS2 UCRT64）；
  - C++ 标准：`-std=c++14`；
  - 源文件：`src/main.cpp`、`src/alg.cpp`、`src/io/config_parser.cpp`；
  - 头文件路径：`src`、`third_party/p4est/build/local/include`、MS-MPI Include、UCRT64 include；
  - 链接库：`-lp4est -lsc -lz -lmsmpi -lws2_32`；
  - 输出：`bin/AMR_Solver.exe`。
- **p4est（third_party）**：Makefile 首次调用 CMake（`-Dmpi=ON -DCMAKE_BUILD_TYPE=Release`）构建并 install 到 `build/local`。

## 结论

- Makefile 编译三个源文件，与当前 `src/` 生产代码一致；
- C++14 标准贯穿 Makefile；
- p4est 作为独立 CMake 库构建，Makefile 仅消费其头文件与静态库，两套构建无源码重复或标准冲突；
- 本次 B1 的 `vector_matrix.h` 别名改动经 G0 验证，Makefile 构建链完整。

## 门禁

- B2 以静态审计收口，无生产代码改动；G0 已由 B1 验证。
