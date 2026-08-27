#pragma once
#include <cstdio>
#include "defines.h"
#include "core/trace.h"

// M12.2: read-only hydro trace records. Numerical callbacks call these helpers
// instead of owning file construction or diagnostic logic.
namespace Diagnostics {

inline void trace_parent_edge_matrix(p4est_iter_volume_info_t *info, int k)
{
	if (!target_trace_enabled()) {
		return;
	}
	p4est_data_t *p4est_data =
		&((P4estBridge *)info->p4est->user_pointer)->data;
	if (p4est_data->current_step != 3 && p4est_data->current_step != 4) {
		return;
	}
	if (!is_trace_parent(info->quad)) {
		return;
	}
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	const ParentBounInfo &pc = data->m_pc_edge_data[k];
	FILE *f = open_corner2_trace(info->p4est);
	if (f) {
		std::fprintf(f, "TRACE stage=PARENT_EDGE_MATRIX step=%d k=%d is_pc=%d\n",
			p4est_data->current_step, k, pc.IsParentChildBoun ? 1 : 0);
		std::fclose(f);
	}
}

} // namespace Diagnostics
