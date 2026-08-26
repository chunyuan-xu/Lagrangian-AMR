#pragma once

#include "defines.h"
#include "nodal/epoch_lifecycle.h"

// Runtime adapter for stamping CellNodalData.stage after a stage's shadow
// writers have refreshed boundaries and geometry.

namespace Nodal {

struct StageResetContext {
	EpochContext ctx;
};

inline EpochContext make_stage_context(const p4est_data_t &data,
	std::uint64_t generation, std::uint64_t topology_version,
	std::uint16_t sub_stage, StagePhase phase)
{
	EpochContext ctx;
	ctx.generation = generation;
	ctx.topology_version = topology_version;
	ctx.step = static_cast<std::uint32_t>(data.current_step >= 0 ? data.current_step : 0);
	ctx.sub_stage = sub_stage;
	ctx.phase = phase;
	return ctx;
}

inline void stamp_stage_reset(quad_data_t &data, const EpochContext &ctx)
{
	stamp_current(data.nodal.stage, ctx);
}

inline void invalidate_stage_reset(quad_data_t &data)
{
	invalidate_stamp(data.nodal.stage);
}

inline EpochError validate_stage_reset(const quad_data_t &data,
	const EpochContext &ctx)
{
	return validate_stamp(data.nodal.stage, ctx);
}

} // namespace Nodal
