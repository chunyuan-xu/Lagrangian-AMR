# M6 M2 IO 模块审计结论（2026-08-07）

## 审计范围

核对 IO 模块（writer 封装）与 main.cpp 的分工，确认文件命名、字段、精度、时间元数据与旧路径一致。

## 现状

- `src/io/vtk_writer.h`：提供 `IOAlgorithm::WriteVTKSolution`（通用 p4est vtk 写入）与 `p4est_debug_output_vtu` 声明；
- `src/io/output_stamp.h`：输出时间戳封装；
- `src/io/config_parser.*`：参数解析；
- `src/main.cpp`：
  - `write_solution`（4225）：生产 VTU 写入（Pressure/density/internal_energy cell + NodeX/NodeY/NodeU/NodeV point），文件名 `output/Lagrangian_%04d`；
  - `p4est_debug_output_vtu`（4590）：调试 VTU 写入，含 Global_SFC_ID；
  - `write_distance_profiles`（4203）：profile 写入。

## 结论

IO 基础设施（writer 封装、output stamp、config parser）已位于 `src/io/`。生产 `write_solution` 仍依赖 main.cpp 的 `quadrant_copy_variable_to_array_callback`（静态，读取 `CVariable` 字段）与 `#ifdef` 分支，直接搬入 header 会因文本级括号匹配与静态回调跨 TU 引用引入高风险。M6.2 以审计确认 IO 拆分边界成立，生产 writer 的完整迁移延期至后续（需先剥离 `#ifdef` 或改为非文本化迁移）。

## 门禁

- 当前 main.cpp 为 M1 干净状态（`f78bf93`），make clean + make 通过（exit 0）。
