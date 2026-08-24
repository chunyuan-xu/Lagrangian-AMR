#pragma once

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <memory>
#include "diagnostics/memory_high_water.h"
#include "diagnostics/ghost_exchange_observer.h"

namespace Diagnostics {

static const std::uint64_t kProjectedDualLayoutPayloadBytes = 7072;

enum class ExchangeOrigin : std::uint8_t {
	Initial = 0,
	Rebuild = 1,
	Ordinary = 2
};

struct MemoryProbeContext {
	MemoryHighWaterTracker *tracker;
	const int *current_step;
	ExchangeOrigin origin;
	bool failure;
	std::uint8_t coverage_bits;
};

inline bool memory_high_water_probe_requested()
{
	const char *value = std::getenv("LAGRANGIAN_MEMORY_HIGH_WATER");
	return value != NULL && std::strcmp(value, "1") == 0;
}

inline std::uint8_t exchange_origin_bit(ExchangeOrigin origin)
{
	switch (origin) {
	case ExchangeOrigin::Initial:
		return 1u << 0;
	case ExchangeOrigin::Rebuild:
		return 1u << 1;
	case ExchangeOrigin::Ordinary:
		return 1u << 2;
	}
	return 0;
}

inline void observe_memory_exchange(void *raw_context,
	const GhostExchangeObservation &observation)
{
	if (raw_context == NULL) {
		assert(raw_context != NULL);
		return;
	}
	MemoryProbeContext &context =
		*static_cast<MemoryProbeContext *>(raw_context);
	if (context.failure) {
		return;
	}

	const std::uint8_t origin_bit = exchange_origin_bit(context.origin);
	if (context.tracker == NULL || context.current_step == NULL ||
		*context.current_step < 0 ||
		observation.payload_bytes != sizeof(quad_data_t) || origin_bit == 0) {
		context.failure = true;
		return;
	}

	const MemoryProbeSample sample = {
		observation.generation,
		static_cast<std::uint64_t>(*context.current_step),
		observation.local_leaves,
		observation.ghost_leaves,
		observation.mirror_leaves,
		observation.logical_send_entries,
		observation.p4est_reported_ghost_bytes
	};
	if (!context.tracker->observe_completed_exchange(sample,
			observation.payload_bytes, kProjectedDualLayoutPayloadBytes)) {
		context.failure = true;
		return;
	}
	context.coverage_bits = static_cast<std::uint8_t>(
		context.coverage_bits | origin_bit);
}

struct MemoryProbeState {
	MemoryHighWaterTracker tracker;
	MemoryProbeContext context;

	explicit MemoryProbeState(const int *current_step)
		: tracker(),
		  context{&tracker, current_step, ExchangeOrigin::Initial, false, 0}
	{
	}
};

class MemoryProbeOwner {
public:
	explicit MemoryProbeOwner(const int *current_step)
		: state_(memory_high_water_probe_requested()
			? new MemoryProbeState(current_step) : NULL)
	{
	}

	bool enabled() const { return state_.get() != NULL; }

	void bind(GhostSession &session)
	{
		if (state_) {
			session.set_exchange_observer(observe_memory_exchange,
				&state_->context);
		}
	}

	MemoryProbeContext *context()
	{
		return state_ ? &state_->context : NULL;
	}

	const MemoryProbeContext *context() const
	{
		return state_ ? &state_->context : NULL;
	}

	MemoryProbeOwner(const MemoryProbeOwner &) = delete;
	MemoryProbeOwner &operator=(const MemoryProbeOwner &) = delete;

private:
	std::unique_ptr<MemoryProbeState> state_;
};

} // namespace Diagnostics
