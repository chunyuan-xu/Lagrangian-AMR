#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <p4est.h>
#include "defines.h"
#include "core/vector_matrix.h"

// M8.2: Trace — shared debug/trace helpers extracted from main.cpp so
// callback modules (hydro/amr/io) can decode trace gating consistently.

inline int g_trace_riemann_iter = -1;

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
