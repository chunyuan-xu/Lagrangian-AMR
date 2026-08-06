#pragma once
#include <p4est.h>
#include <p4est_extended.h>
#include "mesh/ghost_session.h"

// M4.4: AMRController — extracts the AMR stage orchestration from the main
// time loop. Stage order and p4est parameters are identical to the original
// inline block: refine -> rebuild ghost -> coarsen tag -> coarsen -> balance
// -> destroy ghost. Diagnostic logging is kept in the caller.

namespace AMRController {

typedef int (*refine_fn)(p4est_t *, p4est_topidx_t, p4est_quadrant_t *);
typedef int (*coarsen_fn)(p4est_t *, p4est_topidx_t, p4est_quadrant_t **);
typedef void (*replace_fn)(p4est_t *, p4est_topidx_t, int,
	p4est_quadrant_t **, int, p4est_quadrant_t **);
typedef void (*tag_fn)(p4est_t *, GhostSession &);
typedef void (*energy_fn)(p4est_t *);

inline void execute_amr(p4est_t *p4est, GhostSession &session,
	int recursive, int allowed_level, int callbackorphans,
	refine_fn refine_cb, coarsen_fn coarsen_cb, replace_fn replace_cb,
	tag_fn tag_cb, energy_fn energy_cb)
{
	p4est_refine_ext(p4est, recursive, allowed_level,
		refine_cb, NULL, replace_cb);

	session.invalidate_after_topology_change();
	session.rebuild(p4est, P4EST_CONNECT_FULL);

	tag_cb(p4est, session);

	p4est_coarsen_ext(p4est, recursive, callbackorphans,
		coarsen_cb, NULL, replace_cb);

	energy_cb(p4est);
	p4est_balance_ext(p4est, P4EST_CONNECT_CORNER, NULL,
		replace_cb);

	session.invalidate_after_topology_change();
	session.destroy();
}

inline void execute_partition(p4est_t *p4est, GhostSession &session,
	int allowcoarsening)
{
	p4est_partition(p4est, allowcoarsening, NULL);
	session.invalidate_after_topology_change();
	session.destroy();
}

} // namespace AMRController
