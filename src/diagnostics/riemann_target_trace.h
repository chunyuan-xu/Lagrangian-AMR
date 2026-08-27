#pragma once
#include <cstdio>
#include "defines.h"
#include "core/trace.h"
#include "mesh/ghost_session.h"

// M12.1: step-3 Riemann targeted trace moved to diagnostics.
// Disabled mode returns before any mesh traversal.
namespace Diagnostics {

inline int riemann_trace_filename(char *buf, std::size_t size, int mpisize)
{
	return std::snprintf(buf, size, "riemann_dbg_%d.txt", mpisize);
}

inline void dump_riemann_target_if_enabled(p4est_t *p4est,
	GhostSession &session)
{
	if (!target_trace_enabled()) {
		return;
	}
	p4est_data_t *p4est_data =
		&((P4estBridge *)p4est->user_pointer)->data;
	if (p4est_data->current_step != 3) {
		return;
	}

	auto callback = [](p4est_iter_volume_info_t *info, void *user_data) {
		(void)user_data;
		if (!is_trace_fine(info->quad)) {
			return;
		}
		quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
		CVariable *m_vara = &data->m_vara;
		char fname[256];
		const int n = riemann_trace_filename(
			fname, sizeof(fname), info->p4est->mpisize);
		if (n < 0 || n >= static_cast<int>(sizeof(fname))) {
			return;
		}
		FILE *f = std::fopen(fname, "a");
		if (f) {
			std::fprintf(f,
				"RIEMANN_TARGET (x=%d, y=%d, level=%d) corner velocities:\n",
				info->quad->x, info->quad->y, info->quad->level);
			for (int j = 0; j < P4EST_CHILDREN; ++j) {
				std::fprintf(f, "  Corner %d: vx=%f, vy=%f\n", j,
					m_vara->corner_vector(idcnVelocity_cur, j).x,
					m_vara->corner_vector(idcnVelocity_cur, j).y);
			}
			std::fclose(f);
		}
	};

	p4est_iterate(p4est, session.get(), session.data(),
		callback, NULL, NULL);
}

} // namespace Diagnostics
