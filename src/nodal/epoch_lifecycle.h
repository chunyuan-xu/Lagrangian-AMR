#pragma once

#include <cstdint>
#include "nodal/nodal_storage.h"

// L5: Epoch and invalidation lifecycle.  A StageStamp is the sole gate for
// consuming transient nodal data: a valid stamp must match the current
// generation, topology version, step, sub-stage, and phase.  Any mismatch is
// treated as a stale read and rejected by read_guard.

namespace Nodal {

enum class LifecycleEvent : std::uint8_t {
	Invalid = 0,
	Initial = 1,
	Stage = 2,
	Geometry = 3,
	Refine = 4,
	Coarsen = 5,
	Balance = 6,
	Partition = 7,
	GhostRebuild = 8
};

struct EpochContext {
	std::uint64_t generation;
	std::uint64_t topology_version;
	std::uint32_t step;
	std::uint16_t sub_stage;
	StagePhase phase;
};

struct EpochError {
	bool failed;
	const char *reason;
};

inline StageStamp make_stamp(const EpochContext &ctx)
{
	StageStamp stamp;
	stamp.generation = ctx.generation;
	stamp.topology_version = ctx.topology_version;
	stamp.step = ctx.step;
	stamp.sub_stage = ctx.sub_stage;
	stamp.phase = static_cast<std::uint8_t>(ctx.phase);
	stamp.validity = static_cast<std::uint8_t>(ValidityFlag::Valid);
	return stamp;
}

inline void stamp_current(StageStamp &stamp, const EpochContext &ctx)
{
	stamp = make_stamp(ctx);
}

inline void invalidate_stamp(StageStamp &stamp)
{
	stamp.validity = static_cast<std::uint8_t>(ValidityFlag::Invalid);
}

inline EpochError validate_stamp(const StageStamp &stamp,
	const EpochContext &ctx)
{
	if (stamp.validity != static_cast<std::uint8_t>(ValidityFlag::Valid)) {
		return EpochError{true, "stamp invalid"};
	}
	if (stamp.generation != ctx.generation) {
		return EpochError{true, "generation mismatch"};
	}
	if (stamp.topology_version != ctx.topology_version) {
		return EpochError{true, "topology version mismatch"};
	}
	if (stamp.step != ctx.step) {
		return EpochError{true, "step mismatch"};
	}
	if (stamp.sub_stage != ctx.sub_stage) {
		return EpochError{true, "sub-stage mismatch"};
	}
	if (stamp.phase != static_cast<std::uint8_t>(ctx.phase)) {
		return EpochError{true, "phase mismatch"};
	}
	return EpochError{false, nullptr};
}

inline EpochError read_guard(const StageStamp &stamp,
	const EpochContext &ctx)
{
	return validate_stamp(stamp, ctx);
}

} // namespace Nodal
