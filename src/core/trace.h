#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <p4est.h>
#include <p4est_iterate.h>
#include "defines.h"
#include "core/vector_matrix.h"
#include "variable.h"

// M8.2: Trace — shared debug/trace helpers extracted from main.cpp so
// callback modules (hydro/amr/io) can decode trace gating consistently.

inline int &trace_riemann_iter()
{
	static int value = -1;
	return value;
}

inline bool debug_flag_enabled(const char *name)
{
	const char *value = std::getenv(name);
	return value != NULL && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

inline bool target_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_TARGET");
	return enabled;
}

inline bool refine_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_REFINE");
	return enabled;
}

inline bool verbose_amr_log_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_VERBOSE_AMR");
	return enabled;
}

inline bool checksum_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_CHECKSUM");
	return enabled;
}

inline bool refresh_idempotence_check_enabled()
{
	static const bool enabled =
		debug_flag_enabled("LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE");
	return enabled;
}

inline bool state_invariant_check_enabled()
{
	static const bool enabled =
		debug_flag_enabled("LAGRANGIAN_CHECK_STATE_INVARIANTS");
	return enabled;
}

#define AMR_DEBUG_LOG(...) do { \
	if (verbose_amr_log_enabled()) { \
		P4EST_GLOBAL_PRODUCTIONF(__VA_ARGS__); \
	} \
} while (0)

inline FILE *open_corner2_trace(p4est_t *p4est)
{
	if (!target_trace_enabled()) {
		return NULL;
	}
	char fname[256];
	sprintf(fname, "corner2_trace_%d_rank_%d.txt", p4est->mpisize, p4est->mpirank);
	return fopen(fname, "a");
}

inline bool is_trace_fine(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 528482304 && quad->level == 7;
}

inline bool is_trace_sibling(const p4est_quadrant_t *quad)
{
	return quad->x == 142606336 && quad->y == 528482304 && quad->level == 7;
}

inline bool is_trace_parent(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 536870912 && quad->level == 6;
}

inline bool is_trace_refine_parent(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 520093696 && quad->level == 6;
}

inline void trace_matrix(FILE *f, const char *name, const CDoubleMatrix &m)
{
	fprintf(f, " %s=(%.17e,%.17e,%.17e,%.17e)", name, m.xx, m.xy, m.yx, m.yy);
}

inline void trace_vector(FILE *f, const char *name, const CDoubleVector &v)
{
	fprintf(f, " %s=(%.17e,%.17e)", name, v.x, v.y);
}

inline void trace_target_snapshot_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t *p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	const char *stage = static_cast<const char *>(user_data);
	if ((p4est_data->current_step != 2 && p4est_data->current_step != 3) || stage == NULL ||
		(!is_trace_fine(info->quad) && !is_trace_parent(info->quad) && !is_trace_refine_parent(info->quad))) {
		return;
	}
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable *v = &data->m_vara;
	FILE *f = open_corner2_trace(info->p4est);
	if (f) {
		fprintf(f, "TRACE stage=SNAPSHOT step=%d point=%s cell=(%d,%d,L%d)", p4est_data->current_step, stage,
			info->quad->x, info->quad->y, info->quad->level);
		fprintf(f, " rho_cur=%.17e p_cur=%.17e sound=%.17e", v->cell(idDensity_cur), v->cell(idPressure_cur), v->cell(idSoundSpeed));
		for (int c = 0; c < CNDIM; ++c) {
			char name[64];
			sprintf(name, "cur%d", c); trace_vector(f, name, v->corner_vector(idcnVelocity_cur, c));
			sprintf(name, "lag%d", c); trace_vector(f, name, v->corner_vector(idcnVelocity_lag, c));
		}
		fprintf(f, "\n");
		fclose(f);
	}
}

inline void trace_target_snapshot(p4est_t *p4est, const char *stage)
{
	if (!target_trace_enabled()) {
		return;
	}
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	if (p4est_data->current_step != 2 && p4est_data->current_step != 3) {
		return;
	}
	p4est_iterate(p4est, NULL, (void *)stage,
		trace_target_snapshot_callback, NULL, NULL);
}
