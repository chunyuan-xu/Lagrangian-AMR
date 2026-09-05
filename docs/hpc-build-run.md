# Linux 超算编译与运行说明

## 需要复制的内容

只复制 `src/` 和 `include/` 不够。建议直接复制完整仓库，至少包括：

```text
src/
third_party/p4est/       # 若超算没有预装 p4est
CMakeLists.txt
param.ini
```

`bin/`、`build/` 和 `output/` 不需要复制，它们应在超算上重新生成。根目录 `Makefile` 是 Windows/MSYS2 专用，不建议在 Linux 超算上直接使用。

## 加载依赖

不同集群的模块名不同，下面是常见形式：

```bash
module load gcc cmake ninja openmpi zlib
```

确认 MPI 编译器和运行器来自同一套 MPI：

```bash
which mpicxx
which mpirun       # 或 which srun
mpicxx --version
```

## 编译 p4est

如果集群没有 p4est，先在仓库中编译。为保持未压缩 VTU 输出，保留禁用 zlib 检测的选项：

```bash
cmake -S third_party/p4est -B third_party/p4est/build \
  -G Ninja -Dmpi=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE
cmake --build third_party/p4est/build -j 8
cmake --install third_party/p4est/build \
  --prefix third_party/p4est/build/local
```

若集群已有 p4est/libsc，直接记录其安装前缀，跳过这一步。

## 编译求解器

使用新增的根目录 CMake 配置，不使用 Windows 专用 Makefile：

```bash
cmake -S . -B build-hpc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DP4EST_ROOT="$PWD/third_party/p4est/build/local"
cmake --build build-hpc -j 8
test -x build-hpc/AMR_Solver
```

如果 p4est 安装在系统路径中，可省略 `-DP4EST_ROOT`；如果 MPI 不是默认编译器，显式指定：

```bash
cmake -S . -B build-hpc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=mpicxx \
  -DP4EST_ROOT=/path/to/p4est
```

## 运行 Sedov 算例

在项目根目录运行，确保 `param.ini` 位于当前工作目录：

```bash
rm -rf output
mkdir output
mpirun -np 4 ./build-hpc/AMR_Solver
```

Slurm 集群可使用：

```bash
srun --ntasks=4 ./build-hpc/AMR_Solver
```

输出应包含多 rank 的 `.vtu` 和一个 `.pvtu` 文件。建议每次实验使用独立目录保存 `output/`、标准输出和 `param.ini` 快照。

## 常见问题

- `p4est.h not found`：检查 `-DP4EST_ROOT` 是否指向同时含 `include/` 和 `lib/` 的安装前缀。
- `mpi.h not found` 或链接到错误 MPI：确认 `mpicxx`、p4est 和运行器使用同一 MPI 实现。
- `windows.h` 或 `direct.h` 报错：当前源码已对 `windows.h` 做条件包含；`io_callbacks.h` 已有 Linux `mkdir` 分支。
- 只复制 `src/`：缺少根目录构建配置、`param.ini` 和 p4est 安装/源码，无法独立完成编译运行。根目录 `include/` 是旧的 Windows 兼容头集合，Linux 构建不应加入头文件搜索路径。
