# DoD 闭合：配置无隐藏默认继承（2026-08-12）

## 审计范围

核对配置加载路径，确认无隐藏默认继承、回归可重现。

## 现状

- `src/core/simulation_config.h`：`SimulationConfig` 为显式结构体（problem/时间/mesh/solver/output 配置），`valid()` 校验各字段范围与一致性（如 `maximum_level >= minimum_level`、`global_nx > 0` 等）；
- `src/io/config_parser.h`：`ConfigParser` 从 `param.ini` 加载，`p4est_data_t::load_from_config` 显式填充字段；
- `main()`：`ctx.load_from_config(cfg)` → `SC_CHECK_ABORT(ctx.has_valid_simulation_settings(), ...)` 强制校验；
- 每个门禁运行前 `param.ini` SHA-256 恒为 `55bccddd...`，运行后逐字节恢复（`param_restored: true`）。

## 结论

- 配置字段全部显式声明与校验，无隐藏默认继承（未显式指定的字段由 `param.ini` 或结构体默认值决定，且 `valid()` 拦截非法值）；
- 回归可重现：固定 `param.ini` + 固定 reference + 固定门禁 runner 产生确定性结果（G1/G3 全程通过）；
- DoD「配置无隐藏默认继承，回归可重现」已满足。

## 门禁

- 该审计以静态核查收口；G1/G3 已验证参数恢复与黄金一致性。
