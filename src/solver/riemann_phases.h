#pragma once
#include <p4est.h>
#include "mesh/ghost_session.h"

// M5.2: RiemannPhases — explicit phase chain for one Riemann iteration:
// assemble -> exchange -> solve master -> exchange -> solve hanging -> exchange.
// The per-phase functions are supplied by the caller (main.cpp adapters) so
// the sequence, exchange boundaries, and iteration count stay byte-identical.

namespace RiemannPhases {

typedef void (*assemble_fn)(p4est_t *, GhostSession &);
typedef void (*master_solve_fn)(p4est_t *, GhostSession &);
typedef void (*hanging_solve_fn)(p4est_t *, GhostSession &);

inline void run_iteration(p4est_t *p4est, GhostSession &session,
	assemble_fn assemble, master_solve_fn solve_master,
	hanging_solve_fn solve_hanging)
{
	assemble(p4est, session);
	session.exchange();

	solve_master(p4est, session);
	session.exchange();

	solve_hanging(p4est, session);
	session.exchange();
}

} // namespace RiemannPhases
