# VTU/PVTU 黄金输出与比较契约

本文档定义回归测试使用的 VTU/PVTU 文件结构、字段命名、网格对齐、数值比较和资产保护规则。契约版本为 `lagrangian-amr.vtu-pvtu.v1`；这里的版本是项目回归契约版本，不是自行定义的 VTK XML schema 版本。它是 `python/compare_vtu.py` 的使用契约，不要求当前生产代码立即增加新的物理量。

## 1. 文件类型和路径

- `.vtu` 是单个 VTK XML UnstructuredGrid 输出，串行回归通常使用它。
- `.pvtu` 是并行索引文件，通过多个 `Piece Source` 引用 rank 分块 `.vtu`。
- `Source` 按 `.pvtu` 所在目录解析相对路径；当前比较器会对有 `Source` 的 piece 逐个打开引用的 `.vtu`，引用文件缺失或无法解析时比较失败。参考资产和 canonical runner 必须保证每个 piece 都有有效 `Source`。
- 比较器支持 ASCII DataArray 和当前实现支持的 base64 binary DataArray；不承诺其它 VTK XML 编码变体。
- 当前求解器的真实运行输出目录是项目根目录 `output/`，不是 `bin/output/`。
- 串行和 MPI 回归必须选择当前运行产生的末帧，不能让旧 output 残留冒充新结果。

## 2. 拓扑契约

比较器先检查网格规模，再比较字段：

1. `NumberOfPoints` 必须相同；
2. `NumberOfCells` 必须相同；
3. PVTU 比较使用所有 piece 汇总后的点数和单元数；
4. 字段数组 shape 必须一致；
5. 点数或单元数不一致时立即失败，不继续报告字段数值差异。

单纯相同的点/单元数量不证明拓扑身份相同；并行与串行输出优先依赖 `Global_SFC_ID` 对齐，而不是依赖输出顺序或漂移后的物理坐标。

## 3. 字段位置和命名

字段名大小写敏感，必须按照 VTU XML 中的 `Name` 精确匹配。当前生产 writer 的回归字段为：

| 字段 | 位置 | 说明 |
|---|---|---|
| `Pressure` | CellData | 单元压力 |
| `density` | CellData | 单元密度 |
| `internal_energy` | CellData | 当前生产 writer 的内能字段 |
| `NodeX` | PointData | 节点 X 坐标 |
| `NodeY` | PointData | 节点 Y 坐标 |
| `NodeU` | PointData | 节点 X 速度 |
| `NodeV` | PointData | 节点 Y 速度 |

调试或阶段性快照可能使用其它命名，例如 `InternalEnergy`、`Density`、`VelocityU_c0`、`VelocityV_c0` 或 `Global_SFC_ID`。这些名字不能自动与生产字段的大小写变体视为同一字段；需要在比较命令中显式指定正确名称。

`Temperature` 是未来扩展字段。当前代码虽存在温度相关缓存，但生产 writer 尚未把 `Temperature` 作为正式输出字段写入，因此：

- 不把 `Temperature` 加入默认历史回归字段；
- 只有 target 和 reference 都写出同名字段后，才在命令中显式比较它；
- 显式请求缺失的 `Temperature` 必须失败，不能把缺失当作通过；
- 新字段接入时必须重新生成经过批准的参考资产，并重新执行对应 G1/G3 门禁。

## 4. 对齐规则

当 target 和 reference 都包含 `Global_SFC_ID` 时，比较器会：

1. 检查两侧 ID 数组 shape；
2. 分别排序所有字段数组；
3. 检查排序后的 ID 集合逐项相同；
4. 在相同 ID 顺序下比较指定字段。

当前比较器的默认模式会把 `Global_SFC_ID` 作为 `DISCOVERABLE_FIELDS` 中的附加字段：只要 target 和 reference 两侧都有它，就会自动加入默认字段比较；显式 `--fields` 模式则只比较用户列出的字段。若只有一侧存在该字段，比较器会明确提示未进行 ID 对齐；不能把物理坐标当作并行/串行输出的首选稳定键。

## 5. 数值比较规则

- 历史 G1/G3 门禁显式使用绝对容差 `1e-12`。
- 比较器报告每个字段的最大绝对差和相对差。
- 数组 shape 不一致直接失败。
- 缺少被请求字段直接失败。
- 任一字段包含 NaN 或 Inf 直接失败。
- 比较成功返回进程退出码 `0`；解析、拓扑、字段或数值失败返回 `1`。

旧调用保持有效：

```powershell
python python/compare_vtu.py `
  --target output/current.vtu `
  --ref reference/baseline.vtu `
  --tol 1e-12
```

当前默认模式会比较 `density`、`Pressure`，并自动追加 `DISCOVERABLE_FIELDS` 中 target 与 reference 两侧都存在的字段；因此默认字段集合不是固定的“8 个物理场”，会随实际输出字段变化。显式模式只比较列出的字段，不自动追加其它字段；字段名重复或为空会被拒绝。

未来字段可以使用显式严格模式：

```powershell
python python/compare_vtu.py `
  --target output/current.vtu `
  --ref reference/baseline.vtu `
  --tol 1e-12 `
  --fields density Pressure Temperature
```

也支持逗号分隔：

```text
--fields density,Pressure,Temperature
```

显式模式只比较列出的字段，不自动追加其它字段；字段名重复或为空会被拒绝。

## 6. PVTU 参考资产

四进程黄金参考由一个 `.pvtu` 和其引用的 rank 分块 `.vtu` 组成。比较前应确认：

- `.pvtu` 的每个 `Piece Source` 都指向预期文件；
- piece 文件没有被另一个算例覆盖；
- 当前运行和 reference 使用相同的案例、时间步和黄金参数；
- `Global_SFC_ID` 能够覆盖相同的单元身份集合。

不要未经批准重新生成、覆盖或重命名 `reference/` 中的 `.vtu`/`.pvtu`。如果需要更新黄金资产，必须记录原因，并重新完成 G0、相关 G1 和 G3。

## 7. 与回归 runner 的关系

`python/run_tests.py` 和 `python/run_mpi_gates.py` 保持旧的默认 comparator 调用、summary schema、case 顺序、退出码和 `param.ini` 字节恢复语义。比较器字段扩展不会自动改变这些 runner 的字段集合。

runner 的输出目录清理、失败后是否继续下一个 case、末帧发现规则属于独立行为；修订时必须单独设计并验证，不能通过修改参考文件掩盖问题。

## 8. 新字段接入流程

1. 在生产 writer 中稳定输出字段，并确定 PointData/CellData association、数组长度、类型和单位。
2. 为串行和并行输出各生成经过审查的参考文件；不得直接覆盖旧基线。
3. 先用 `--fields` 显式验证新字段和现有字段。
4. 增加 `.vtu` 与 `.pvtu` 的正向、缺字段、shape 和非有限值测试。
5. 完成对应 G1/G3 全量回归后，才能考虑把新字段纳入默认契约。

## 9. 不可变回归条件

- `param.ini` 在 runner 结束后必须与运行前逐字节一致。
- summary JSON 的 schema 和状态含义不因比较器扩展而改变。
- `reference/` 是只读黄金资产，除非有明确批准不得修改。
- `output/` 是临时运行产物，不是参考结果。
- 构建临时文件、`.o`、日志和旧输出不能被误当成回归证据。
