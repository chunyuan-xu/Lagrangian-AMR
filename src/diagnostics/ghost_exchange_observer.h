#pragma once

#include <cassert>
#include <cstdint>
#include "mesh/ghost_session.h"

namespace Diagnostics {

inline GhostExchangeObservation capture_exchange_schedule(
	const GhostSession &session, p4est_t *forest)
{
	p4est_ghost_t *ghost = session.get();
	const GhostExchangeObservation observation = {
		static_cast<std::uint64_t>(session.generation()),
		static_cast<std::uint64_t>(forest->local_num_quadrants),
		static_cast<std::uint64_t>(ghost->ghosts.elem_count),
		static_cast<std::uint64_t>(ghost->mirrors.elem_count),
		static_cast<std::uint64_t>(
			ghost->mirror_proc_offsets[forest->mpisize]),
		static_cast<std::uint64_t>(forest->data_size),
		static_cast<std::uint64_t>(p4est_ghost_memory_used(ghost))
	};
	return observation;
}

inline void exchange_observed(GhostSession &session, p4est_t *forest,
	GhostExchangeObserver observer, void *observer_context)
{
	assert(observer != NULL);
	const GhostExchangeObservation observation =
		capture_exchange_schedule(session, forest);
	session.exchange();
	observer(observer_context, observation);
}

inline void observe_completed_schedule(GhostSession &session, p4est_t *forest)
{
	assert(session.has_exchange_observer());
	const GhostExchangeObservation observation =
		capture_exchange_schedule(session, forest);
	session.exchange_observer()(
		session.exchange_observer_context(), observation);
}

inline void exchange_selected(GhostSession &session, p4est_t *forest)
{
	if (!session.has_exchange_observer()) {
		session.exchange();
		return;
	}
	exchange_observed(session, forest, session.exchange_observer(),
		session.exchange_observer_context());
}

inline void initialize_selected(GhostSession &session, p4est_t *forest,
	p4est_connect_type_t connectivity)
{
	session.initialize(forest, connectivity);
	if (session.has_exchange_observer()) {
		observe_completed_schedule(session, forest);
	}
}

inline void rebuild_selected(GhostSession &session, p4est_t *forest,
	p4est_connect_type_t connectivity)
{
	session.rebuild(forest, connectivity);
	if (session.has_exchange_observer()) {
		observe_completed_schedule(session, forest);
	}
}

} // namespace Diagnostics
