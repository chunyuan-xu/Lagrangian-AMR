#include "alg.h"
#include "amr/amr_criteria.h"
#include "solver/corner_solver.h"
#include "solver/solver_gate.h"
#include "io/vtk_writer.h"
#include "io/config_parser.h"
#include "io/output_stamp.h"
#include "physics/timestep_reduction.h"
#include "physics/stage_policy.h"
#include "diagnostics/state_invariant_checker.h"
#include "mesh/ghost_session.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include<sys/stat.h>
#include<sys/types.h>
#endif 
using namespace std;

static int g_trace_riemann_iter = -1;

struct GhostCallbackContext
{
	GhostSession *session;
};

static bool debug_flag_enabled(const char *name)
{
	const char *value = std::getenv(name);
	return value != NULL && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

static bool target_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_TARGET");
	return enabled;
}

static bool refine_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_REFINE");
	return enabled;
}

static bool verbose_amr_log_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_VERBOSE_AMR");
	return enabled;
}

static bool checksum_trace_enabled()
{
	static const bool enabled = debug_flag_enabled("LAGRANGIAN_TRACE_CHECKSUM");
	return enabled;
}

static bool refresh_idempotence_check_enabled()
{
	static const bool enabled =
		debug_flag_enabled("LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE");
	return enabled;
}

static bool state_invariant_check_enabled()
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

static FILE *open_corner2_trace(p4est_t *p4est)
{
	if (!target_trace_enabled()) {
		return NULL;
	}
	char fname[256];
	sprintf(fname, "corner2_trace_%d_rank_%d.txt", p4est->mpisize, p4est->mpirank);
	return fopen(fname, "a");
}

static bool is_trace_fine(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 528482304 && quad->level == 7;
}

static bool is_trace_sibling(const p4est_quadrant_t *quad)
{
	return quad->x == 142606336 && quad->y == 528482304 && quad->level == 7;
}

static bool is_trace_parent(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 536870912 && quad->level == 6;
}

static bool is_trace_refine_parent(const p4est_quadrant_t *quad)
{
	return quad->x == 134217728 && quad->y == 520093696 && quad->level == 6;
}

static void trace_matrix(FILE *f, const char *name, const CDoubleMatrix &m)
{
	fprintf(f, " %s=(%.17e,%.17e,%.17e,%.17e)", name, m.xx, m.xy, m.yx, m.yy);
}

static void trace_vector(FILE *f, const char *name, const CDoubleVector &v)
{
	fprintf(f, " %s=(%.17e,%.17e)", name, v.x, v.y);
}

#ifndef P4_TO_P8
#include<p4est_vtk.h>
#include<p4est_bits.h>
#include<p4est_extended.h>
#include<p4est_iterate.h>
#include<p4est_io.h>
#include<p4est_communication.h>
#include<windows.h>
#else
#include<p8est_vtk.h>
#include<p8est_bits.h>
#include<p8est_extended.h>
#include<p8est_iterate.h>
#include<p8est_io.h>
#include<p8est_communication.h>
#endif 

static const char *g_trace_snapshot_stage = NULL;

static void trace_target_snapshot_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t *p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	if ((p4est_data->current_step != 2 && p4est_data->current_step != 3) || g_trace_snapshot_stage == NULL ||
		(!is_trace_fine(info->quad) && !is_trace_parent(info->quad) && !is_trace_refine_parent(info->quad))) {
		return;
	}
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable *v = &data->m_vara;
	FILE *f = open_corner2_trace(info->p4est);
	if (f) {
		fprintf(f, "TRACE stage=SNAPSHOT step=%d point=%s cell=(%d,%d,L%d)", p4est_data->current_step, g_trace_snapshot_stage,
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

static void trace_target_snapshot(p4est_t *p4est, const char *stage)
{
	if (!target_trace_enabled()) {
		return;
	}
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	if (p4est_data->current_step != 2 && p4est_data->current_step != 3) {
		return;
	}
	g_trace_snapshot_stage = stage;
	p4est_iterate(p4est, NULL, NULL, trace_target_snapshot_callback, NULL, NULL);
	g_trace_snapshot_stage = NULL;
}


static void get_hanging_edge_info_from_logical_position(const int which_face, const p4est_qcoord_t qx1, const p4est_qcoord_t qy1,
	const p4est_qcoord_t qx2, const p4est_qcoord_t qy2, const p4est_qcoord_t length,
	int which_corner[2], int which_side[2], int master_corner[2], int unconstrained_master_corner[2])
{
	switch (which_face)
	{
	case quad_data_t::EnumEdge::LEFT:
		if (qy1 == qy2 + length)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::plus;

			
			which_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else if (qy1 + length == qy2)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[0] = CHalf_edge_data::cside::minus;

			
			which_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::RIGHT:
		if (qy1 == qy2 + length)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::minus;

			
			which_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else if (qy1 + length == qy2)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			which_side[0] = CHalf_edge_data::cside::plus;

			
			which_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::BOTTOM:
		if (qx1 == qx2 + length)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			which_side[0] = CHalf_edge_data::cside::minus;

			
			which_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else if (qx1 + length == qx2)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[0] = CHalf_edge_data::cside::plus;

			
			which_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::UP:
		if (qx1 == qx2 + length)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::plus;

			
			which_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else if (qx1 + length == qx2)
		{
			
			which_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::minus;

			
			which_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	default:
		P4EST_GLOBAL_PRODUCTIONF("the value of which_face must be between 0 and 3!");
		break;
	}
}


static void quadrant_compute_RcpLcpNcp_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				SolverType = p4est_data->solver_type;
	CHalf_edge_data	*m_plus, *m_minus;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;

	
	CDoubleVector EdgeMiddle[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		EdgeMiddle[k] = 0.5 * (m_vara->corner_vector(idcnCoords_cur, k) + m_vara->corner_vector(idcnCoords_cur, knext));
	}
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		int kpre = GeometryAlg::GetCirclePre(CNDIM, k);
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		if (p4est_data->coord_type == p4est_data_t::MyCoordType::plane)
		{
			m_plus->Rcp = 1.0;
			m_minus->Rcp = 1.0;
		}
		else
		{
			if (SolverType == p4est_data_t::RiemannSolver::GridAligned)
			{
				m_plus->Rcp = GeometryAlg::GetRcpWeightWithLinearDistribution(m_vara->corner_vector(idcnCoords_cur, k),EdgeMiddle[k]);
				m_minus->Rcp = GeometryAlg::GetRcpWeightWithLinearDistribution(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[kpre]);
			}
			if (SolverType == p4est_data_t::RiemannSolver::Rotated)
			{
				m_plus->Rcp = GeometryAlg::GetRcpWeightWithConstDistribution(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[k]);
				m_minus->Rcp = GeometryAlg::GetRcpWeightWithConstDistribution(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[kpre]);
			}
		}

		m_plus->Lcp = GeometryAlg::GetPointToPointDistance(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[k]);
		m_minus->Lcp = GeometryAlg::GetPointToPointDistance(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[kpre]);

		
		m_plus->Ncp = GeometryAlg::GetLineNormalVector(m_vara->corner_vector(idcnCoords_cur, k), EdgeMiddle[k]);

		
		m_minus->Ncp = GeometryAlg::GetLineNormalVector(EdgeMiddle[kpre], m_vara->corner_vector(idcnCoords_cur, k));
	}
}


static void quadrant_compute_relaxed_info_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				SolverType = p4est_data->solver_type;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;

	double			delta_time = p4est_data->dt_iter;
	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->corner_vector(idcnCoords_relaxed, k) = m_vara->corner_vector(idcnCoords_half, k) +
			CDoubleVector(m_vara->corner_vector(idcnVelocity_relaxed, k).x * delta_time, m_vara->corner_vector(idcnVelocity_relaxed, k).y * delta_time);
	}
}


static void quadrant_relaxed_hanging_solver_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = context->session->data();
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	const CVariable	*m_child1_read_vara, *m_child2_read_vara, *m_parent_read_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];
	
	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if ((side[i]->is.hanging.is_ghost[0]
				&& !context->session->valid_remote_id(
					side[i]->is.hanging.quadid[0]))
				|| (side[i]->is.hanging.is_ghost[1]
				&& !context->session->valid_remote_id(
					side[i]->is.hanging.quadid[1]))) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;
			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_read_vara = &ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_read_vara = &m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_read_vara = &ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_read_vara = &m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_read_vara = &ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_read_vara = &m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector	master_velocity[2];
			switch (parent_face_index)
			{
				case quad_data_t::EnumEdge::LEFT:
					master_velocity[0] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
					master_velocity[1] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
					break;
				case quad_data_t::EnumEdge::RIGHT:
					master_velocity[0] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
					master_velocity[1] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
					break;
				case quad_data_t::EnumEdge::BOTTOM:
					master_velocity[0] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
					master_velocity[1] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
					break;
				case quad_data_t::EnumEdge::UP:
					master_velocity[0] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
					master_velocity[1] = m_parent_read_vara->corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
					break;
			}

			CDoubleVector hanging_velo = 0.5 * (master_velocity[0] + master_velocity[1]);

			if (target_trace_enabled() && p4est_data->current_step == 3 &&
				((is_trace_fine(quad_child1) && is_trace_sibling(quad_child2)) ||
				 (is_trace_sibling(quad_child1) && is_trace_fine(quad_child2)))) {
				FILE *f = open_corner2_trace(info->p4est);
				if (f) {
					fprintf(f, "TRACE stage=HANGING_OVERRIDE iter=%d child0=(%d,%d,L%d,c%d,g%d) child1=(%d,%d,L%d,c%d,g%d) parent=(%d,%d,L%d,face%d,g%d)",
						g_trace_riemann_iter, quad_child1->x, quad_child1->y, quad_child1->level, m_which_corner[0], side[i]->is.hanging.is_ghost[0] ? 1 : 0,
						quad_child2->x, quad_child2->y, quad_child2->level, m_which_corner[1], side[i]->is.hanging.is_ghost[1] ? 1 : 0,
						quad_parent->x, quad_parent->y, quad_parent->level, parent_face_index, side[full_index]->is.full.is_ghost ? 1 : 0);
					trace_vector(f, "master0", master_velocity[0]);
					trace_vector(f, "master1", master_velocity[1]);
					trace_vector(f, "before0", m_child1_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[0]));
					trace_vector(f, "before1", m_child2_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[1]));
					trace_vector(f, "hanging", hanging_velo);
					fprintf(f, "\n");
					fclose(f);
				}
			}

			if (!side[i]->is.hanging.is_ghost[0]) {
				m_child1_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[0]) = hanging_velo;
			}
			if (!side[i]->is.hanging.is_ghost[1]) {
				m_child2_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[1]) = hanging_velo;
			}

			CDoubleMatrix MatrixP = m_child1_data->points[m_which_corner[0]].MatrixP;
			CDoubleVector m_rhs = m_child1_data->points[m_which_corner[0]].RHS;
			CDoubleVector Flux_relaxed;
			Flux_relaxed.x = MatrixP.xx * hanging_velo.x + MatrixP.xy * hanging_velo.y - m_rhs.x;
			Flux_relaxed.y = MatrixP.yx * hanging_velo.x + MatrixP.yy * hanging_velo.y - m_rhs.y;

			double child1_total_energy = m_child1_read_vara->cell(idMass) * m_child1_read_vara->cell(idTotalEnergy_cur);
			double child2_total_energy = m_child2_read_vara->cell(idMass) * m_child2_read_vara->cell(idTotalEnergy_cur);
			double parent_total_energy = m_parent_read_vara->cell(idMass) * m_parent_read_vara->cell(idTotalEnergy_cur);


			if (!side[i]->is.hanging.is_ghost[0]) {
				m_child1_vara->corner_vector(idcnFluxRelaxed, m_which_corner[0]) = child1_total_energy /
					(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
			}
			if (!side[i]->is.hanging.is_ghost[1]) {
				m_child2_vara->corner_vector(idcnFluxRelaxed, m_which_corner[1]) = child2_total_energy /
					(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
			}
			if (!side[full_index]->is.full.is_ghost) {
				ParentBounInfo  *PCInfo = (ParentBounInfo  *)&m_parent_data->m_pc_edge_data;
				m_parent_data->m_pc_edge_data[parent_face_index].IsParentChildBoun = true;
				PCInfo[parent_face_index].addDiss = true;
				PCInfo[parent_face_index].Hanging_velocity = hanging_velo;
				PCInfo[parent_face_index].FluxRelaxed = parent_total_energy /
					(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
			}
		}
	}
}


static int convert_which_corner_to_user_define_index(const int &which_corner)
{
	int m_index;
	if (which_corner == p4est_enum_corner::left_bottom) { m_index = quad_data_t::EnumCorner::LEFTBOTTOM; }
	if (which_corner == p4est_enum_corner::right_bottom) { m_index = quad_data_t::EnumCorner::RIGHTBOTTOM; }
	if (which_corner == p4est_enum_corner::left_up) { m_index = quad_data_t::EnumCorner::LEFTUP; }
	if (which_corner == p4est_enum_corner::right_up) { m_index = quad_data_t::EnumCorner::RIGHTUP; }
	return m_index;
}

static int convert_user_define_index_to_which_corner(const int &which_corner)
{
	int m_index;
	if (which_corner == 0) { m_index = 0; }
	if (which_corner == 3) { m_index = 1; }
	if (which_corner == 1) { m_index = 2; }
	if (which_corner == 2) { m_index = 3; }
	return m_index;
}


void CalculateCornerRcpLcpNcp(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_RcpLcpNcp_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void quadrant_get_BYD_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	CEdge_data		*edata = (CEdge_data *)&data->m_edata;
	p4est_t			*p4est = info->p4est;
	p4est_connectivity_t *conn = p4est->connectivity;
	CHalf_edge_data  *m_plus, *m_minus;
	int			level = info->quad->level;
	int			quadid = info->quadid;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_qcoord_t qx = info->quad->x;
	p4est_qcoord_t qy = info->quad->y;
	p4est_topidx_t	which_tree = info->treeid;


	p4est_qcoord_to_vertex(conn, which_tree, qx, qy, data->init_node_coords[0]);
	p4est_qcoord_to_vertex(conn, which_tree, qx, qy+length, data->init_node_coords[1]);
	p4est_qcoord_to_vertex(conn, which_tree, qx+length, qy+length, data->init_node_coords[2]);
	p4est_qcoord_to_vertex(conn, which_tree, qx+length, qy, data->init_node_coords[3]);

	double x_length = p4est_data->m_grid_info.global_nx*p4est_data->m_grid_info.tree_width;
	double y_length = p4est_data->m_grid_info.global_ny*p4est_data->m_grid_info.tree_height;
	bool m_left_boundary = false;
	bool m_right_boundary = false;
	bool m_bottom_boundary = false;
	bool m_top_boundary = false;
	
	
	int ix = info->treeid%p4est_data->x_tree_number;
	int iy = info->treeid / p4est_data->y_tree_number;

	for (int i = 0; i < CNDIM; i++){
		
		m_plus = (CHalf_edge_data *)&cndata[i].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[i].hdata[CHalf_edge_data::cside::minus];
		m_plus->enumBYD = -1;
		m_minus->enumBYD = -1;
		m_plus->BYDVal = 0.;
		m_minus->BYDVal = 0.;
	}
	

	if (p4est_data->which_case == ProblemNo::TriplePoint)
	{
		if (data->init_node_coords[0][0] == 0)
		{
			m_left_boundary = true;
		}
		if (data->init_node_coords[2][0] == x_length)
		{
			m_right_boundary = true;
		}
		if (data->init_node_coords[0][1] == 0)
		{
			m_bottom_boundary = true;
		}
		if (data->init_node_coords[2][1] == y_length)
		{
			m_top_boundary = true;
		}
	}
	else
	{
		if (qx == 0) { m_left_boundary = true; }
		if (qx == P4EST_ROOT_LEN - length) { m_right_boundary = true; }
		if (qy == 0) { m_bottom_boundary = true; }
		if (qy == P4EST_ROOT_LEN - length) { m_top_boundary = true; }
	}

	if (m_left_boundary)
	{
		cndata[0].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->LeftBoun;
		cndata[1].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->LeftBoun;

		cndata[0].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->LeftBounVal;
		cndata[1].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->LeftBounVal;

		edata[quad_data_t::EnumEdge::LEFT].EdgeType = p4est_data->LeftBoun;
	}

	
	if (m_right_boundary)
		
	{
		cndata[3].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->RightBoun;
		cndata[2].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->RightBoun;

		cndata[3].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->RightBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->RightBounVal;

		edata[quad_data_t::EnumEdge::RIGHT].EdgeType = p4est_data->RightBoun;
	}

	
	if (m_bottom_boundary)
		
	{
		cndata[0].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->BottomBoun;
		cndata[3].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->BottomBoun;

		cndata[0].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->BottomBounVal;
		cndata[3].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->BottomBounVal;

		edata[quad_data_t::EnumEdge::BOTTOM].EdgeType = p4est_data->BottomBoun;
	}

	
	if (m_top_boundary)
		
	{
		cndata[1].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->TopBoun;
		cndata[2].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->TopBoun;

		cndata[1].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->TopBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->TopBounVal;

		edata[quad_data_t::EnumEdge::UP].EdgeType = p4est_data->TopBoun;
	}
}


static void get_boundary_from_p4est(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	PhysicalAlg::InitBoundaryCondition(p4est_data->which_case,
		p4est_data->coord_type,
		p4est_data->TopBoun,
		p4est_data->BottomBoun,
		p4est_data->LeftBoun,
		p4est_data->RightBoun,
		p4est_data->TopBounVal,
		p4est_data->BottomBounVal,
		p4est_data->LeftBounVal,
		p4est_data->RightBounVal);

	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_get_BYD_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void get_quadrant_boundary_from_p4est(p4est_t *p4est, p4est_quadrant_t *q)
{
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;


	for (int i = 0; i < CNDIM; i++) {

		cndata[i].hdata[CHalf_edge_data::cside::minus].enumBYD = -1;
		cndata[i].hdata[CHalf_edge_data::cside::plus].enumBYD = -1;
		cndata[i].hdata[CHalf_edge_data::cside::minus].BYDVal = 0.;
		cndata[i].hdata[CHalf_edge_data::cside::plus].BYDVal = 0.;
	}
	

	if (qx==0)
	{
		cndata[0].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->LeftBoun;
		cndata[1].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->LeftBoun;
		cndata[0].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->LeftBounVal;
		cndata[1].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->LeftBounVal;
	}

	
	if (qx == P4EST_ROOT_LEN - length)
	{
		cndata[3].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->RightBoun;
		cndata[2].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->RightBoun;
		cndata[3].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->RightBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->RightBounVal;
	}

	
	if(qy==0)
	{
		cndata[0].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->BottomBoun;
		cndata[3].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->BottomBoun;
		cndata[0].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->BottomBounVal;
		cndata[3].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->BottomBounVal;
	}

	
	if(qy==P4EST_ROOT_LEN - length)
	{
		cndata[1].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->TopBoun;
		cndata[2].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->TopBoun;
		cndata[1].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->TopBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->TopBounVal;
	}
}

static void quadrant_vtk_coord_update_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_connectivity_t *connectivity = info->p4est->connectivity;
	p4est_qcoord_t qx = info->quad->x;
	p4est_qcoord_t qy = info->quad->y;
	int			level = info->quad->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_topidx_t	which_tree = info->treeid;
	double			new_node_coords[CNDIM][P4EST_DIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		new_node_coords[cnid][0] = m_vara->corner_vector(idcnCoords_lag, cnid).x;
		new_node_coords[cnid][1] = m_vara->corner_vector(idcnCoords_lag, cnid).y;
	}
	int m_size = connectivity->num_vertices;
	for(int i = 0; i <m_size*3;i++)
	{
		printf("Vertex %d, %lf\n", i, connectivity->vertices[i]);
	}
}

static int Lagrangian_init_refine_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	DoubleCellVariableID idCPara;
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	if (qx <= 4 * length ||
		qy <= 4 * length)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lagrangian_refine_fixed_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	DoubleCellVariableID idCPara;
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		// idCentroidCoord_cur is a VectorCellVariableID, incompatible with the
		// DoubleCellVariableID idCPara. idCPara is never read in this function,
		// so the assignment was dead code and is dropped.

	default:
		break;
	}


	if (qx <= 12 * length || qy <= 12 * length)
	
	
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lagrangian_coarsen_fixed_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data;
	

	DoubleCellVariableID idCPara;
	double		parent_gradient;

	
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	
	parent_gradient = 0.;
	for (int i = 0; i < P4EST_CHILDREN; i++)
	{
		data = (quad_data_t *)children[i]->p.user_data;
		p4est_qcoord_t qx = children[i]->x;
		p4est_qcoord_t qy = children[i]->y;
		int			level = children[i]->level;
		p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);


		if (qx >= 18 * length || qy >= 18 * length)
		{
			return 1;
		}
	}
	return 0;
}

static int Lagrangian_refine_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	return AMRAgorithm::RefineErrorEstimate(p4est, which_tree, q);
}

static int Lagrangian_coarsen_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	return AMRAgorithm::CoarsenErrorEstimate(p4est, which_tree, children);
}
static int Lagrangian_init_coarsen_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data;

	DoubleCellVariableID idCPara;
	double		parent_gradient;

	
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	parent_gradient = 0.;
	for (int i = 0; i < P4EST_CHILDREN; i++)
	{
		data = (quad_data_t *)children[i]->p.user_data;
		p4est_qcoord_t qx = children[i]->x;
		p4est_qcoord_t qy = children[i]->y;
		int			level = children[i]->level;
		p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

		if (level <= p4est_data->minus_level)
		{
			return 0;
		}

		if (level > p4est_data->max_level)
		{
			return 1;
		}

		
		if (data->m_vara.cell(idCPara) > p4est_data->coarsen_error) { return 0; }
		parent_gradient += data->m_vara.cell(idCPara);
	}
	parent_gradient /= P4EST_CHILDREN;
	if (parent_gradient > p4est_data->coarsen_error) { return 0; }
	else { return 1; }
}

static int Lagrangian_coarsen_init_condition(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_quadrant_t parent;


	p4est_quadrant_parent(children[0], &parent);

	return 0;
}

static void generate_children_info_from_parent(p4est_data_t *p4est_data, CVariable *m_vara)
{
	
	
	CDoubleVector EdgeData[4], CenterData, CenterCoord, CenterVelocity, concave_center;
	CDoubleVector m_parent_coord[CNDIM], m_parent_concave_coord[CNDIM],
		m_parent_velo[CNDIM], m_parent_concave_velo[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_parent_coord[cnid] = m_vara->corner_vector(idcnCoords_lag, cnid);
		m_parent_concave_coord[cnid] = m_vara->corner_vector(idcnCoords_lag, CNDIM-1-cnid);

		m_parent_velo[cnid] = m_vara->corner_vector(idcnVelocity_lag, cnid);
		m_parent_concave_velo[cnid] = m_vara->corner_vector(idcnVelocity_lag, CNDIM - 1 - cnid);
	}

	int IsConcaveQuad = GeometryAlg::is_concave_quad(m_parent_concave_coord);

	if (IsConcaveQuad < 0)
	{
		if (p4est_data->children_center_type == p4est_data_t::center_type::average)
		{
			CenterCoord = 0.25 * (m_parent_coord[0] + m_parent_coord[1] +
				m_parent_coord[2] + m_parent_coord[3]);
		}
		else if (p4est_data->children_center_type == p4est_data_t::center_type::integrated)
		{
			CenterCoord = GeometryAlg::GetPolyCenter(m_parent_coord);
		}
		CenterVelocity = 0.25 * (m_parent_velo[0] + m_parent_velo[1] +
			m_parent_velo[2] + m_parent_velo[3]);
	}
	else
	{
		CenterCoord = GeometryAlg::concave_quad_centroid(IsConcaveQuad, m_parent_concave_coord);
		CenterVelocity = 0.5 * (m_parent_concave_velo[IsConcaveQuad]
			+ m_parent_concave_velo[(IsConcaveQuad+2)%4]);
	}

	enum m_geometry_id { m_coord, m_velo };
	enum m_physical_id {m_density, m_internal_energy};
	enum m_which_child {child1, child2, child3, child4};
	VectorCornerVariableID idCnIndex;
	DoubleCellVariableID   idCIndex;
	for (int idChildrenGeomIndex = m_geometry_id::m_coord; idChildrenGeomIndex <= m_geometry_id::m_velo; idChildrenGeomIndex++)
	{
		switch (idChildrenGeomIndex)
		{
		case m_geometry_id::m_coord:
			idCnIndex = idcnCoords_lag;
			CenterData = CenterCoord;
			break;
		case m_geometry_id::m_velo:
			idCnIndex = idcnVelocity_lag;
			CenterData = CenterVelocity;
			break;
		default:
			break;
		}

		
		EdgeData[quad_data_t::EnumEdge::LEFT] = 0.5*
			(m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTBOTTOM) +
				m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTUP));
		EdgeData[quad_data_t::EnumEdge::BOTTOM] = 0.5*
			(m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTBOTTOM) +
				m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTBOTTOM));
		EdgeData[quad_data_t::EnumEdge::RIGHT] = 0.5*
			(m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTBOTTOM) +
				m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTUP));
		EdgeData[quad_data_t::EnumEdge::UP] = 0.5*
			(m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTUP) +
				m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTUP));


		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::LEFTBOTTOM] =
			m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTBOTTOM);
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::LEFTUP] =
			EdgeData[quad_data_t::EnumEdge::LEFT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::RIGHTUP] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::RIGHTBOTTOM]=
			EdgeData[quad_data_t::EnumEdge::BOTTOM];
		
		
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::LEFTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::BOTTOM];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::LEFTUP] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::RIGHTUP] =
			EdgeData[quad_data_t::EnumEdge::RIGHT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTBOTTOM);

		
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::LEFTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::LEFT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::LEFTUP] =
			m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::LEFTUP);
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::RIGHTUP] =
			EdgeData[quad_data_t::EnumEdge::UP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			CenterData;

		
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::LEFTBOTTOM] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::LEFTUP] =
			EdgeData[quad_data_t::EnumEdge::UP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::RIGHTUP] =
			m_vara->corner_vector(idCnIndex, quad_data_t::EnumCorner::RIGHTUP);
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::RIGHT];
	}

	CDoubleVector m_child1_coord[CNDIM], m_child2_coord[CNDIM],
		m_child3_coord[CNDIM], m_child4_coord[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{

		m_child1_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child1][cnid];
		m_child2_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child2][cnid];
		m_child3_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child3][cnid];
		m_child4_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child4][cnid];
	}
	double parent_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_parent_coord);
	double child1_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child1_coord);
	double child2_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child2_coord);
	double child3_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child3_coord);
	double child4_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child4_coord);
	double children_volume = child1_volume + child2_volume + child3_volume + child4_volume;

	if (abs((parent_volume - children_volume) / parent_volume) > m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("The pre-calculated children information is wrong\n");
		abort();
	}

	for (int idChildrenPhysicalIndex = m_physical_id::m_density; idChildrenPhysicalIndex <= m_physical_id::m_internal_energy; idChildrenPhysicalIndex++)
	{
		switch (idChildrenPhysicalIndex)
		{
		case m_physical_id::m_density:
			idCIndex = idDensity_lag;
			break;
		case m_physical_id::m_internal_energy:
			idCIndex = idInternalEnergy_lag;
			break;
		default:
			break;
		}
		for (int childid = 0; childid < P4EST_CHILDREN; childid++)
		{
			m_vara->ChildrenPhysicalVara[idChildrenPhysicalIndex][childid] =
				m_vara->cell(idCIndex);
			if (m_vara->ChildrenPhysicalVara[idChildrenPhysicalIndex][childid] > m_eps)
			{
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("The value of ChildrenPysicalVara is illegal in refining!\n");
				abort();
			}
		}
	}
}


static void Lagrangian_init_condition(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{

	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	p4est_connectivity_t *connectivity = p4est->connectivity;
	p4est_data_t			*p4est_data = (p4est_data_t *)p4est->user_pointer;

	p4est_data->coord_type = p4est_data_t::MyCoordType::plane;
	p4est_data->Scheme_type = p4est_data_t::MySchemeType::ControlVolume;


	int			level = q->level;

	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	
	double dx = 1.0 / (1 << level);


	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;

	int index_i = int(qx / length);
	int index_j = int(qy / length);
	int width_num = (1 << level);

	
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy, data->init_node_coords[0]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy + length, data->init_node_coords[1]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy + length, data->init_node_coords[2]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy, data->init_node_coords[3]);

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->corner_vector(idcnCoords_cur, i).x = data->init_node_coords[i][0];
		m_vara->corner_vector(idcnCoords_cur, i).y = data->init_node_coords[i][1];
		m_vara->corner_vector(idcnVelocity_cur, i) = CDoubleVector(0.0, 0.0);
		m_vara->corner_vector(idcnVelocity_lag, i) = CDoubleVector(0.0, 0.0);
	}

	CDoubleVector cnCoordCur[CNDIM], cnCoordLag[CNDIM], cnVeloCur[CNDIM], cnVeloLag[CNDIM];
	for (int i = 0; i < CNDIM; i++)
	{
		cnCoordCur[i] = m_vara->corner_vector(idcnCoords_cur, i);
		cnCoordLag[i] = m_vara->corner_vector(idcnCoords_lag, i);
		cnVeloCur[i] = m_vara->corner_vector(idcnVelocity_cur, i);
		cnVeloLag[i] = m_vara->corner_vector(idcnVelocity_lag, i);
	}

	PhysicalAlg::InitCondition(p4est_data->which_case,
		p4est_data->coord_type, int(qx), int(qy), index_i, index_j, width_num,
		cnCoordCur, cnCoordLag, cnVeloCur, cnVeloLag,
		m_vara->cell(idDensity_cur),
		m_vara->cell(idDensity_lag),
		m_vara->cell(idVolume),
		m_vara->cell(idMass),
		m_vara->cell_vector(idCentroidCoord_cur),
		m_vara->cell_vector(idCentroidCoord_lag),
		m_vara->cell_vector(idCentroidVelo_cur),
		m_vara->cell_vector(idCentroidVelo_lag),
		m_vara->cell(idInternalEnergy_cur),
		m_vara->cell(idInternalEnergy_lag),
		m_vara->cell(idPressure_cur),
		m_vara->cell(idPressure_lag),
		m_vara->cell(idTotalEnergy_cur),
		m_vara->cell(idTotalEnergy_lag),
		m_vara->cell(idSoundSpeed),
		m_vara->cell(idGamma),
		p4est_data->TopBoun,
		p4est_data->BottomBoun,
		p4est_data->LeftBoun,
		p4est_data->RightBoun,
		p4est_data->TopBounVal,
		p4est_data->BottomBounVal,
		p4est_data->LeftBounVal,
		p4est_data->RightBounVal);

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->corner_vector(idcnCoords_cur, i) = cnCoordCur[i];
		m_vara->corner_vector(idcnCoords_lag, i) = cnCoordLag[i];
		m_vara->corner_vector(idcnVelocity_cur, i) = cnVeloCur[i];
		m_vara->corner_vector(idcnVelocity_lag, i) = cnVeloLag[i];
	}

	generate_children_info_from_parent(p4est_data, m_vara);
}


static void quadrant_predict_timestep_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	CDoubleVector corner_coords[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		corner_coords[cnid] = m_vara->corner_vector(idcnCoords_cur, cnid);
	}

	if (m_vara->cell(idSoundSpeed) < m_eps)
	{

	}
	
	
	double quad_cfl_dt = PhysicalAlg::get_CourantTimeStep(
		corner_coords, m_vara->cell(idSoundSpeed));

	
	double quad_vol_dt = PhysicalAlg::get_VolumeVarationTimeStep(
		p4est_data->volume_varation_torelarion,
		m_vara->cell(idDivergence));

	
	double quad_increased_dt = p4est_data->delta_time * p4est_data->dt_increase_percent;

	
	const double quad_dt = min(quad_cfl_dt,
		min(quad_vol_dt, quad_increased_dt));
	p4est_data->local_dt = TimestepReduction::accumulate_local_minimum(
		p4est_data->local_dt, quad_dt);

	if (p4est_data->local_dt < m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("Time step is too small in quad %d\n", info->quadid);
		abort();
	}
}


static void 
predict_timestep(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_data->local_dt = TimestepReduction::initial_local_minimum();

	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_predict_timestep_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	int		mpiret;
	mpiret =
		sc_MPI_Allreduce(&p4est_data->local_dt, &p4est_data->delta_time,
			1, sc_MPI_DOUBLE, sc_MPI_MIN, p4est->mpicomm);
	SC_CHECK_MPI(mpiret);
}


static void quadrant_compute_divergence_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	
	
	CDoubleVector	cnVelocity[CNDIM];
	CDoubleVector	cnCoord[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		cnCoord[k] = m_vara->corner_vector(idcnCoords_lag, k);
		cnVelocity[k] = m_vara->corner_vector(idcnVelocity_lag, k);
	}
	m_vara->cell(idDivergence) = PhysicalAlg::CalculateDivergence(p4est_data->coord_type, cnCoord, cnVelocity);
}


void ComputeDivergence(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_divergence_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}


static void quadrant_compute_soundspeed_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
		m_vara->cell(idGamma),
		m_vara->cell(idPressure_lag),
		m_vara->cell(idDensity_lag));
}


static void 
quadrant_corner_minmod_estimate_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	which_corner, cnid, is_ghost, is_ghost_aside, m_size;
	int			quadid, quadid_aside;
	DoubleCellVariableID idCPara;
	DoubleCornerVariableID idCNPara;
	quad_data_t		*m_data, *m_data_aside;
	CVariable		*m_vara, *m_vara_aside;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;
	double			ParaGradient;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idPressure_cur;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idDensity_cur;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	m_size = int(sides->elem_count);

	
	for (int i = 0; i < m_size; i++)
	{
		
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		
		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_vara = (CVariable  *)&m_data->m_vara;

		if (!is_ghost) {
			m_vara->corner(idCNPara, cnid) = 0.;
		}
		for (int j = 0; j < m_size; j++)
		{
			if (j == i) { continue; }
			side[j] = p4est_iter_cside_array_index_int(sides, j);
			quadid_aside = side[j]->quadid;
			is_ghost_aside = side[j]->is_ghost;
			if (is_ghost_aside)
			{
				m_data_aside = (quad_data_t  *)&ghost_data[quadid_aside];
			}
			else
			{
				m_data_aside = (quad_data_t  *)side[j]->quad->p.user_data;
			}
			m_vara_aside = (CVariable  *)&m_data_aside->m_vara;

			double m_dist = GeometryAlg::GetPointToPointDistance(
				m_vara->cell_vector(idCentroidCoord_cur), m_vara_aside->cell_vector(idCentroidCoord_cur));
			ParaGradient = abs(m_vara->cell(idCPara) - m_vara_aside->cell(idCPara)) / m_dist;
			if (!is_ghost) {
				m_vara->corner(idCNPara, cnid) = SC_MAX(m_vara->corner(idCNPara, cnid), ParaGradient);
			}
		}
	}
}

static void
quadrant_whether_allowing_coarsening_from_corner_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	sc_array_t	*sides = &(info->sides);
	int	is_ghost_a, m_size;
	int			quadid_a, quadid_b;
	quad_data_t		*m_data_a;
	CVariable		*m_vara_a;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;

	m_size = (int)(sides->elem_count);

	for (int i = 0; i < m_size; i++)
	{
		p4est_iter_corner_side_t *side_i = p4est_iter_cside_array_index_int(sides, i);
		quadid_a = side_i->quadid;
		p4est_quadrant	*quad_a = side_i->quad;
		int level_a = quad_a->level;

		is_ghost_a = side_i->is_ghost;
		if (is_ghost_a)
		{
			if (!ghost_data) {
				P4EST_GLOBAL_PRODUCTIONF("SEGV incoming! ghost_data is NULL!\n");
				abort();
			}
			m_data_a = (quad_data_t  *)&ghost_data[quadid_a];
		}
		else
		{
			m_data_a = (quad_data_t  *)side_i->quad->p.user_data;
		}
		m_vara_a = (CVariable  *) &m_data_a->m_vara;
		
		for (int j = 0; j < m_size; j++)
		{
			if (j == i) { continue; }
			p4est_iter_corner_side_t *side_j = p4est_iter_cside_array_index_int(sides, j);
			quadid_b = side_j->quadid;
			p4est_quadrant	*quad_b = side_j->quad;
			int level_b = quad_b->level;
			
			if (level_b - level_a > 1)
			{
				m_vara_a->int_cell(idAllowCoarsening) = p4est_data_t::CoarseningEnum::CoarsingNotAllowed;
			}
		}
	}
}


static void quadrant_edge_minmod_estimate_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = context->session->data();
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;
	const CVariable		*m_child1_read_vara, *m_child2_read_vara, *m_parent_read_vara;
	CVariable		*m_child1_write_vara = NULL, *m_child2_write_vara = NULL, *m_parent_write_vara = NULL;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	DoubleCellVariableID idCPara;
	DoubleEdgeVariableID idEPara;
	int				m_which_corner[2], m_master_corner[2], 
		m_unconstrained_master_corner[2], m_which_side[2];

	if (sides->elem_count != 2) { return; }
	P4EST_ASSERT(sides->elem_count == 2);

	
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idPressure_cur;
		idEPara = idEPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idDensity_cur;
		idEPara = idERhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1-i] = p4est_iter_fside_array_index_int(sides, 1-i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if ((side[i]->is.hanging.is_ghost[0]
				&& !context->session->valid_remote_id(side[i]->is.hanging.quadid[0]))
				|| (side[i]->is.hanging.is_ghost[1]
				&& !context->session->valid_remote_id(side[i]->is.hanging.quadid[1]))) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_read_vara = &context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_read_vara = &m_child1_data->m_vara;
				m_child1_write_vara = &m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_read_vara = &context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_read_vara = &m_child2_data->m_vara;
				m_child2_write_vara = &m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_read_vara = &context->session->remote(side[full_index]->is.full.quadid).m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_read_vara = &m_parent_data->m_vara;
				m_parent_write_vara = &m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}
			double		parent_para, child1_para, child2_para;
			double		parent_gradient, child1_gradient, child2_gradient;
			double		dist1, dist2;
			CDoubleVector	parent_center, child1_center, child2_center;
			int				children_face, parent_face;

			
			parent_para = m_parent_read_vara->cell(idCPara);
			child1_para = m_child1_read_vara->cell(idCPara);
			child2_para = m_child2_read_vara->cell(idCPara);

			
			parent_center = m_parent_read_vara->cell_vector(idCentroidCoord_cur);
			child1_center = m_child1_read_vara->cell_vector(idCentroidCoord_cur);
			child2_center = m_child2_read_vara->cell_vector(idCentroidCoord_cur);

			
			dist1 = GeometryAlg::GetPointToPointDistance(parent_center, child1_center);
			dist2 = GeometryAlg::GetPointToPointDistance(parent_center, child2_center);

			
			child1_gradient = abs(parent_para - child1_para) / dist1;
			child2_gradient = abs(parent_para - child2_para) / dist2;
			parent_gradient = (child1_gradient + child2_gradient) / 2.;

			
			children_face = side[i]->face;
			parent_face = side[1 - i]->face;

			if (m_child1_write_vara != NULL)
				m_child1_write_vara->edge(idEPara, children_face) = child1_gradient;
			if (m_child2_write_vara != NULL)
				m_child2_write_vara->edge(idEPara, children_face) = child2_gradient;
			if (m_parent_write_vara != NULL)
				m_parent_write_vara->edge(idEPara, parent_face) = parent_gradient;
		}
	}

	
	side[0] = p4est_iter_fside_array_index_int(sides, 0);
	side[1] = p4est_iter_fside_array_index_int(sides, 1);
	if (!(side[0]->is_hanging) && !(side[1]->is_hanging))
	{
		double	m_para[2];
		double	m_gradient;
		CDoubleVector	m_center[2];
		int				face_index[2];

		p4est_quadrant_t		*brother1_quad, *brother2_quad;
		quad_data_t				*brother1_data, *brother2_data;
		const CVariable			*brother1_read_vara, *brother2_read_vara;
		CVariable				*brother1_write_vara = NULL, *brother2_write_vara = NULL;
		brother1_quad = side[0]->is.full.quad;
		brother2_quad = side[1]->is.full.quad;

		face_index[0] = side[0]->face;

		
		if (side[0]->is.full.is_ghost)
		{
			brother1_read_vara = &context->session->remote(side[0]->is.full.quadid).m_vara;
		}
		else
		{
			brother1_data = (quad_data_t  *)side[0]->is.full.quad->p.user_data;
			brother1_read_vara = &brother1_data->m_vara;
			brother1_write_vara = &brother1_data->m_vara;
		}

		
		if (side[1]->is.full.quad == NULL || info->sides.elem_count <2 ||
			side[1]->is.full.quadid>info->p4est->global_num_quadrants)
		{
			if (brother1_write_vara != NULL)
			{
				brother1_write_vara->edge(idEPara, face_index[0]) = 0.;
			}
			return;
		}

		face_index[1] = side[1]->face;
		if (side[1]->is.full.is_ghost)
		{
			brother2_read_vara = &context->session->remote(side[1]->is.full.quadid).m_vara;
		}
		else if (!(side[1]->is.full.quad))
		{
			if (brother1_write_vara != NULL)
			{
				brother1_write_vara->edge(idEPara, face_index[0]) = 0.;
			}
			return;
		}
		else
		{
			brother2_data = (quad_data_t  *)side[1]->is.full.quad->p.user_data;
			brother2_read_vara = &brother2_data->m_vara;
			brother2_write_vara = &brother2_data->m_vara;
		}

		m_para[0] = brother1_read_vara->cell(idCPara);
		m_para[1] = brother2_read_vara->cell(idCPara);
		m_center[0] = brother1_read_vara->cell_vector(idCentroidCoord_cur);
		m_center[1] = brother2_read_vara->cell_vector(idCentroidCoord_cur);

		double dist = GeometryAlg::GetPointToPointDistance(m_center[0], m_center[1]);
		m_gradient = abs(m_para[0] - m_para[1]) / dist;

		if (brother1_write_vara != NULL)
		{
			brother1_write_vara->edge(idEPara, face_index[0]) = m_gradient;
		}
		if (brother2_write_vara != NULL)
		{
			brother2_write_vara->edge(idEPara, face_index[1]) = m_gradient;
		}
	}
}


static void quadrant_update_after_coarsening_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2],
		m_unconstrained_master_corner[2], m_which_side[2];

	
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants) {

				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector  master_coord[2], master_velo[2], middle_coord, middle_velo,
				child1_cn_coord, child2_cn_coord, child1_cn_velo, child2_cn_velo;

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
				break;
			}
			middle_coord = 0.5*(master_coord[0] + master_coord[1]);
			middle_velo = 0.5*(master_velo[0] + master_velo[1]);

			child1_cn_coord = m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]);
			child2_cn_coord = m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]);

			child1_cn_velo = m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]);
			child2_cn_velo = m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]);

			double dist1, dist2, delta_velo1, delta_velo2;
			dist1 = GeometryAlg::GetPointToPointDistance(middle_coord, child1_cn_coord);
			dist2 = GeometryAlg::GetPointToPointDistance(middle_coord, child2_cn_coord);
			delta_velo1 = GeometryAlg::GetPointToPointDistance(middle_velo, child1_cn_velo);
			delta_velo2 = GeometryAlg::GetPointToPointDistance(middle_velo, child2_cn_velo);

			
			if (delta_velo1 >= m_coliner_eps || delta_velo2 >= m_coliner_eps)
			{
				CDoubleVector  m_cell_coord[CNDIM];

				
				m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]) = middle_coord;
				m_child1_vara->corner_vector(idcnCoords_lag, m_which_corner[0]) = m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]);
				
				m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]) = middle_velo;
				m_child1_vara->corner_vector(idcnVelocity_lag, m_which_corner[0]) = m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]);

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child1_vara->corner_vector(idcnCoords_cur, i); }
				m_child1_vara->cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child1_vara->cell(idDensity_cur) = m_child1_vara->cell(idMass) / m_child1_vara->cell(idVolume);

				m_child1_vara->cell(idPressure_cur) = PhysicalAlg::EquationOfState(
					m_child1_vara->cell(idGamma),
					m_child1_vara->cell(idDensity_cur),
					m_child1_vara->cell(idInternalEnergy_cur));

																   
				m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]) = middle_coord;
				m_child2_vara->corner_vector(idcnCoords_lag, m_which_corner[1]) = m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]);

				m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]) = middle_velo;
				m_child2_vara->corner_vector(idcnVelocity_lag, m_which_corner[1]) = m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]);

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child2_vara->corner_vector(idcnCoords_cur, i); }
				m_child2_vara->cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child2_vara->cell(idDensity_cur) = m_child2_vara->cell(idMass) / m_child2_vara->cell(idVolume);

				m_child2_vara->cell(idPressure_cur) = PhysicalAlg::EquationOfState(
					m_child2_vara->cell(idGamma),
					m_child2_vara->cell(idDensity_cur),
					m_child2_vara->cell(idInternalEnergy_cur));
			}
		}
	}
}

static void quadrant_whether_allowing_coarsening_from_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_parent_data;
	CVariable		*m_parent_vara;
	CCorner_data	*m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);

	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			

			int			childlevel = quad_child1->level;
			int			parentlevel = quad_parent->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(childlevel);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;


			if (side[full_index]->is.full.is_ghost)
			{
				continue;
			}
			m_parent_data = (quad_data_t *)quad_parent->p.user_data;
			m_parent_vara = (CVariable *)&m_parent_data->m_vara;
			m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;

			if ((childlevel - parentlevel) > 1)
			{
				
				m_parent_vara->int_cell(idAllowCoarsening) = p4est_data_t::CoarseningEnum::CoarsingNotAllowed;
			}
		}
	}
}


static void quadrant_update_after_balance_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = context->session->data();
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2],
		m_unconstrained_master_corner[2], m_which_side[2];
	if (sides->elem_count != 2) { return; }
	
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if ((side[i]->is.hanging.is_ghost[0]
				&& !context->session->valid_remote_id(
					side[i]->is.hanging.quadid[0]))
				|| (side[i]->is.hanging.is_ghost[1]
				&& !context->session->valid_remote_id(
					side[i]->is.hanging.quadid[1]))) {

				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector  master_coord[2], master_velo[2], middle_coord, middle_velo,
				child1_cn_coord, child2_cn_coord, child1_cn_velo, child2_cn_velo;

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTBOTTOM);
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);
				master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);

				master_velo[0] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::LEFTUP);
				master_velo[1] = m_parent_data->m_vara.corner_vector(idcnVelocity_lag, quad_data_t::EnumCorner::RIGHTUP);
				break;
			}
			middle_coord = 0.5*(master_coord[0] + master_coord[1]);
			middle_velo = 0.5*(master_velo[0] + master_velo[1]);

			child1_cn_coord = m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]);
			child2_cn_coord = m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]);

			child1_cn_velo = m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]);
			child2_cn_velo = m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]);

			double dist1, dist2, delta_velo1, delta_velo2;
			dist1 = GeometryAlg::GetPointToPointDistance(middle_coord, child1_cn_coord);
			dist2 = GeometryAlg::GetPointToPointDistance(middle_coord, child2_cn_coord);
			delta_velo1 = GeometryAlg::GetPointToPointDistance(middle_velo, child1_cn_velo);
			delta_velo2 = GeometryAlg::GetPointToPointDistance(middle_velo, child2_cn_velo);

			
			if (delta_velo1 >= m_coliner_eps || delta_velo2 >= m_coliner_eps)
			{
				CDoubleVector  m_cell_coord[CNDIM];

				if (!side[i]->is.hanging.is_ghost[0])
				{
					m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]) = middle_coord;
					m_child1_vara->corner_vector(idcnCoords_lag, m_which_corner[0]) = m_child1_vara->corner_vector(idcnCoords_cur, m_which_corner[0]);

					m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]) = middle_velo;
					m_child1_vara->corner_vector(idcnVelocity_lag, m_which_corner[0]) = m_child1_vara->corner_vector(idcnVelocity_cur, m_which_corner[0]);

					for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child1_vara->corner_vector(idcnCoords_cur, i); }
					m_child1_vara->cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
					m_child1_vara->cell(idDensity_cur) = m_child1_vara->cell(idMass) / m_child1_vara->cell(idVolume);

					m_child1_vara->cell(idPressure_cur) = PhysicalAlg::EquationOfState(
						m_child1_vara->cell(idGamma),
						m_child1_vara->cell(idDensity_cur),
						m_child1_vara->cell(idInternalEnergy_cur));
				}

				if (!side[i]->is.hanging.is_ghost[1])
				{
					m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]) = middle_coord;
					m_child2_vara->corner_vector(idcnCoords_lag, m_which_corner[1]) = m_child2_vara->corner_vector(idcnCoords_cur, m_which_corner[1]);

					m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]) = middle_velo;
					m_child2_vara->corner_vector(idcnVelocity_lag, m_which_corner[1]) = m_child2_vara->corner_vector(idcnVelocity_cur, m_which_corner[1]);

					for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child2_vara->corner_vector(idcnCoords_cur, i); }
					m_child2_vara->cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
					m_child2_vara->cell(idDensity_cur) = m_child2_vara->cell(idMass) / m_child2_vara->cell(idVolume);

					m_child2_vara->cell(idPressure_cur) = PhysicalAlg::EquationOfState(
						m_child2_vara->cell(idGamma),
						m_child2_vara->cell(idDensity_cur),
						m_child2_vara->cell(idInternalEnergy_cur));
				}
			}
		}
	}
}

static void quadrant_cell_minmod_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data=(quad_data_t		*)info->quad->p.user_data;
	CVariable		*m_vara=(CVariable		*)&data->m_vara;
	p4est_t			*p4est = info->p4est;

	DoubleCellVariableID idCPara;
	DoubleEdgeVariableID idEPara;
	DoubleCornerVariableID idCNPara;
	
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		idEPara = idEPressureGradient;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		idEPara = idERhoGradient;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	m_vara->cell(idCPara) = 0.;

	
	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->cell(idCPara) = SC_MAX(m_vara->cell(idCPara), m_vara->edge(idEPara, i));
	}


}


void ComputeSoundSpeed(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_soundspeed_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}


static void quadrant_compute_halftime_variable_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t		*)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable		*)&data->m_vara;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;
	m_vara->cell_vector(idCentroidVelo_half) = (m_vara->cell_vector(idCentroidVelo_cur) + m_vara->cell_vector(idCentroidVelo_lag)) / 2.;
	m_vara->cell_vector(idCentroidCoord_half) = (m_vara->cell_vector(idCentroidCoord_cur) + m_vara->cell_vector(idCentroidCoord_lag)) / 2.;

	
	for (int i = 0; i < CNDIM; i++)
	{

		m_vara->corner_vector(idcnCoords_half, i) = GeometryAlg::GetPointPointMiddle(m_vara->corner_vector(idcnCoords_cur, i), m_vara->corner_vector(idcnCoords_lag, i));
	}
	
	m_vara->cell(idDensity_half) = 2.*m_vara->cell(idDensity_cur) * m_vara->cell(idDensity_lag) /
		(m_vara->cell(idDensity_cur) + m_vara->cell(idDensity_lag));
	
	m_vara->cell(idTotalEnergy_half) = (m_vara->cell(idTotalEnergy_cur)+ m_vara->cell(idTotalEnergy_lag)) / 2.;
	m_vara->cell(idInternalEnergy_half) = (m_vara->cell(idInternalEnergy_cur) + m_vara->cell(idInternalEnergy_lag)) / 2.;
	if (m_vara->cell(idInternalEnergy_half) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The half time idinternalenergy is illegal\n", P4EST_DIM);
		abort();
	}
	m_vara->cell(idPressure_half) = PhysicalAlg::EquationOfState(m_vara->cell(idGamma), m_vara->cell(idDensity_half), m_vara->cell(idInternalEnergy_half));
	if (m_vara->cell(idPressure_half) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The half time pressure is illegal\n", P4EST_DIM);
		abort();
	}
}

void CalculateHalfTimeVariable(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;


	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_halftime_variable_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}


static void quadrant_parent_edge_matrix_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t		*)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable		*)&data->m_vara;
	ParentBounInfo	*PCInfo = (ParentBounInfo	*)&data->m_pc_edge_data;
	p4est_data_t *p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	CDoubleVector DeltaU[CNDIM], RcpLcpNcpPc[CNDIM];
	CDoubleMatrix  NcpPlusMatrix, NcpMinusMatrix;

	for (int k = 0; k < CNDIM; k++)
	{
		
		if (PCInfo[k].IsParentChildBoun == true)
		{
			if (target_trace_enabled() && (p4est_data->current_step == 3 || p4est_data->current_step == 4)) {
				char fname[256];
				sprintf(fname, "edge_matrix_dbg_%d_%d.txt", info->p4est->mpisize, info->p4est->mpirank);
				FILE* f_dbg = fopen(fname, "a");
				if (f_dbg) {
					if (info->p4est->mpisize == 1 && info->quadid == 397) {
						fprintf(f_dbg, "STEP %d: SERIAL 397 found! quad->x=%d, quad->y=%d, k=%d, IsParentChildBoun=true\n",
							p4est_data->current_step, info->quad->x, info->quad->y, k);
					} else {
						// For parallel, we just print everything for now to find the matching x and y
						// Or just let's log any quadrant whose x and y matches a known suspicious value
						// But for now, just print the serial 397 to see its x and y.
					}
					fclose(f_dbg);
				}
			}
			double Divergence = 0.;
			CDoubleVector	LcpNcpPc, LcpNcp;
			m_vara->corner(idReconstructDensity, k) = m_vara->cell(idDensity_cur);
			m_vara->corner(idReconstructPressure, k) = m_vara->cell(idPressure_cur);
			m_vara->corner_vector(idReconstructVelocity, k) = m_vara->cell_vector(idCentroidVelo_cur);
			double Tc = 0.;
			DeltaU[k] = PCInfo[k].Hanging_velocity - m_vara->corner_vector(idReconstructVelocity, k);
			LcpNcp = PCInfo[k].Lcp[0] * PCInfo[k].Ncp[0] + PCInfo[k].Lcp[1] * PCInfo[k].Ncp[1];
			LcpNcpPc = LcpNcp * m_vara->corner(idReconstructPressure, k);
			Divergence = LcpNcpPc ^ DeltaU[k];
			if (Divergence < -1e-10) { Tc = 1.44; }
			PCInfo[k].Zcp = m_vara->corner(idReconstructDensity, k) * m_vara->cell(idSoundSpeed);
			NcpPlusMatrix = GeometryAlg::DyadicProduct(PCInfo[k].Ncp[0], PCInfo[k].Ncp[0]);
			NcpMinusMatrix = GeometryAlg::DyadicProduct(PCInfo[k].Ncp[1], PCInfo[k].Ncp[1]);
			m_vara->MarCnData[ideMcp][k] = PCInfo[k].Zcp * PCInfo[k].Lcp[0] * NcpPlusMatrix
				+ PCInfo[k].Zcp * PCInfo[k].Lcp[1] * NcpMinusMatrix;
			m_vara->corner_vector(ideMcpUc, k) = GeometryAlg::MatrixDotVector(m_vara->MarCnData[ideMcp][k],
				m_vara->corner_vector(idReconstructVelocity, k));
			m_vara->corner_vector(ideRHS, k) = LcpNcpPc + m_vara->corner_vector(ideMcpUc, k);
			if (target_trace_enabled() && p4est_data->current_step == 3 && is_trace_parent(info->quad)) {
				FILE *f = open_corner2_trace(info->p4est);
				if (f) {
					fprintf(f, "TRACE stage=PARENT_EDGE iter=%d face=%d is_pc=%d", g_trace_riemann_iter, k, PCInfo[k].IsParentChildBoun ? 1 : 0);
					trace_vector(f, "hanging_in", PCInfo[k].Hanging_velocity);
					fprintf(f, " L=(%.17e,%.17e)", PCInfo[k].Lcp[0], PCInfo[k].Lcp[1]);
					trace_vector(f, "N0", PCInfo[k].Ncp[0]);
					trace_vector(f, "N1", PCInfo[k].Ncp[1]);
					fprintf(f, " Z=%.17e", PCInfo[k].Zcp);
					trace_vector(f, "delta_u", DeltaU[k]);
					trace_matrix(f, "ideMcp", m_vara->MarCnData[ideMcp][k]);
					trace_vector(f, "ideRHS", m_vara->corner_vector(ideRHS, k));
					fprintf(f, "\n");
					fclose(f);
				}
			}
		}
	}
}

static void quadrant_corner_matrix_assemble_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&(data->m_cndata);
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				Scheme_type = p4est_data->Scheme_type;
	CDoubleVector DeltaU[CNDIM], RcpLcpNcpPc[CNDIM];
	CDoubleMatrix  NcpPlusMatrix, NcpMinusMatrix;
	CHalf_edge_data	*m_plus, *m_minus;
	double abs_deltau;

	for (int k = 0; k < CNDIM; k++)
	{
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		double Divergence = 0.;
		CDoubleVector	LcpNcpPc, LcpNcp;

		m_vara->corner(idReconstructDensity, k) = m_vara->cell(idDensity_cur);
		m_vara->corner(idReconstructPressure, k) = m_vara->cell(idPressure_cur);
		double Tc = 0.;
		m_vara->corner_vector(idReconstructVelocity, k) = m_vara->cell_vector(idCentroidVelo_cur);
		DeltaU[k] = m_vara->corner_vector(idcnVelocity_lag, k) - m_vara->corner_vector(idReconstructVelocity, k);
		abs_deltau = sqrt(pow(DeltaU[k].x, 2) + pow(DeltaU[k].y, 2));
		LcpNcp = m_plus->Lcp * m_plus->Ncp + m_minus->Lcp*m_minus->Ncp;
		LcpNcpPc = LcpNcp * m_vara->corner(idReconstructPressure, k);
		Divergence = LcpNcpPc ^ DeltaU[k];

		if (CoordType == p4est_data_t::MyCoordType::plane)
		{
			RcpLcpNcpPc[k] = m_plus->Rcp*m_plus->Lcp * m_plus->Ncp + 
				m_minus->Rcp*m_minus->Lcp*m_minus->Ncp;
		}
		if (CoordType == p4est_data_t::MyCoordType::cylinder)
		{

		}

		m_plus->Zcp = m_vara->corner(idReconstructDensity, k) * m_vara->cell(idSoundSpeed);
		m_minus->Zcp = m_vara->corner(idReconstructDensity, k) * m_vara->cell(idSoundSpeed);
		m_plus->delta_u_cp = m_vara->corner_vector(idcnVelocity_lag, k) - m_vara->corner_vector(idReconstructVelocity, k);
		m_minus->delta_u_cp = m_vara->corner_vector(idcnVelocity_lag, k) - m_vara->corner_vector(idReconstructVelocity, k);
		m_plus->Uc_cur = m_vara->corner_vector(idReconstructVelocity, k);
		m_minus->Uc_cur = m_vara->corner_vector(idReconstructVelocity, k);
		m_plus->pi = m_vara->corner(idReconstructPressure, k) - m_plus->Zcp * (m_plus->delta_u_cp^ m_plus->Ncp);
		m_minus->pi = m_vara->corner(idReconstructPressure, k) - m_minus->Zcp * (m_minus->delta_u_cp^ m_minus->Ncp);

		NcpPlusMatrix = GeometryAlg::DyadicProduct(m_plus->Ncp, m_plus->Ncp);
		NcpMinusMatrix = GeometryAlg::DyadicProduct(m_minus->Ncp, m_minus->Ncp);

		
		m_vara->MarCnData[idcnMcp][k] = m_plus->Zcp*m_plus->Rcp*m_plus->Lcp*NcpPlusMatrix
			+ m_minus->Zcp*m_minus->Rcp*m_minus->Lcp*NcpMinusMatrix;

		
		if (p4est_data->solver_type == p4est_data_t::RiemannSolver::Rotated)
		{
			if (abs_deltau > m_eps)
			{
				m_vara->MarCnData[idcnMcp][k].xx = m_plus->Zcp*m_plus->Rcp*m_plus->Lcp *
					abs(DeltaU[k]^ m_plus->Ncp) / abs_deltau +
					m_minus->Zcp*m_minus->Rcp*m_minus->Lcp *
					abs(DeltaU[k] ^ m_minus->Ncp) / abs_deltau;
				m_vara->MarCnData[idcnMcp][k].yy = m_vara->MarCnData[idcnMcp][k].xx;
				m_vara->MarCnData[idcnMcp][k].xy = 0.;
				m_vara->MarCnData[idcnMcp][k].yx = 0.;
			}
		}


		m_vara->corner_vector(idcnMcpUc, k) = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnMcp][k],
			m_vara->corner_vector(idReconstructVelocity, k));
		m_vara->corner_vector(idcnRHS, k) = LcpNcpPc + m_vara->corner_vector(idcnMcpUc, k);
		if (target_trace_enabled() && p4est_data->current_step == 3 && is_trace_parent(info->quad) && (k == 0 || k == 3)) {
			FILE *f = open_corner2_trace(info->p4est);
			if (f) {
				fprintf(f, "TRACE stage=LOCAL_CORNER iter=%d corner=%d", g_trace_riemann_iter, k);
				trace_matrix(f, "idcnMcp", m_vara->MarCnData[idcnMcp][k]);
				trace_vector(f, "idcnRHS", m_vara->corner_vector(idcnRHS, k));
				trace_vector(f, "velocity_in", m_vara->corner_vector(idcnVelocity_lag, k));
				fprintf(f, "\n");
				fclose(f);
			}
		}
	}
}


static void
quadrant_corner_to_point_matrix_assemble_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	which_corner, cnid, is_ghost, m_size;
	int			quadid;
	bool		is_boundary;
	int			tree_boundary;
	quad_data_t		*m_data;
	CVariable		*m_vara;
	CDoubleMatrix	MatrixP = CDoubleMatrix(0., 0., 0., 0.);
	CDoubleVector	RHS = CDoubleVector(0., 0.);
	CPointBounInfo	OneBounPlus, OneBounMinus;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;

	m_size = int(sides->elem_count);

	for (int i = 0; i < m_size; i++)
	{
		
		side[i] = p4est_iter_cside_array_index_int(sides, i);


		quadid = side[i]->quadid;

		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		
		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_vara = (CVariable  *)&m_data->m_vara;

		
		MatrixP += m_vara->MarCnData[idcnMcp][cnid];
		RHS += m_vara->corner_vector(idcnRHS, cnid);
	}

	for (int i = 0; i < m_size; i++)
	{
		
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		if (!is_ghost) m_data->points[cnid].MatrixP = MatrixP;
		if (!is_ghost) m_data->points[cnid].RHS = RHS;
	}

	tree_boundary = info->tree_boundary;


	m_size = int(sides->elem_count);
	CHalf_edge_data	*m_plus, *m_minus;
	is_boundary = false;
	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];
		if (m_plus->enumBYD != InnerBoundary) { is_boundary = true; }
		if (m_minus->enumBYD != InnerBoundary) { is_boundary = true; }
	}

	
	if (is_boundary)
	{
		vector<CPointBounInfo> m_bouns;
		CHalf_edge_data	*m_plus, *m_minus;
		m_size = int(sides->elem_count);
		for (int i = 0; i < m_size; i++)
		{
			side[i] = p4est_iter_cside_array_index_int(sides, i);
			quadid = side[i]->quadid;
			which_corner = side[i]->corner;
			cnid = convert_which_corner_to_user_define_index(which_corner);

			is_ghost = side[i]->is_ghost;
			if (is_ghost)
			{
				m_data = (quad_data_t  *)&ghost_data[quadid];
			}
			else
			{
				m_data = (quad_data_t  *)side[i]->quad->p.user_data;
			}
			m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
			m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];

			if (m_plus->enumBYD != InnerBoundary)
			{
				OneBounPlus.enumType = m_plus->enumBYD;
				OneBounPlus.Val = m_plus->BYDVal;
				OneBounPlus.Ncp = m_plus->Ncp;
				OneBounPlus.Lcp = m_plus->Lcp;
				m_bouns.push_back(OneBounPlus);
			}
			if (m_minus->enumBYD != InnerBoundary)
			{
				OneBounMinus.enumType = m_minus->enumBYD;
				OneBounMinus.Val = m_minus->BYDVal;
				OneBounMinus.Ncp = m_minus->Ncp;
				OneBounMinus.Lcp = m_minus->Lcp;
				m_bouns.push_back(OneBounMinus);
			}
		}
		if (m_bouns.size() != 2)
		{
			P4EST_GLOBAL_PRODUCTIONF("WARNING:boundary number must be 2!\n");
		}
		else
		{
			for (int i = 0; i < m_size; i++)
			{
				side[i] = p4est_iter_cside_array_index_int(sides, i);
				quadid = side[i]->quadid;
				which_corner = side[i]->corner;
				cnid = convert_which_corner_to_user_define_index(which_corner);

				is_ghost = side[i]->is_ghost;
				if (is_ghost)
				{
					m_data = (quad_data_t  *)&ghost_data[quadid];
				}
				else
				{
					m_data = (quad_data_t  *)side[i]->quad->p.user_data;
				}

				if (!is_ghost) m_data->points[cnid].TwoBouns[0] = m_bouns[0];
				if (!is_ghost) m_data->points[cnid].TwoBouns[1] = m_bouns[1];
			}
		}
		m_bouns.clear();
	}
}


CDoubleVector BoundaryNodeVelocityComputation(const CPointBounInfo &BounPlus,
	const CPointBounInfo &BounMinus, const CDoubleMatrix MatrixP, const CDoubleVector m_RHS)
{
	int				enumPlus = BounPlus.enumType;
	int				enumMinus = BounMinus.enumType;
	double			ValPlus = BounPlus.Val;
	double			ValMinus = BounMinus.Val;
	CDoubleVector	NcpPlus = BounPlus.Ncp;
	CDoubleVector	NcpMinus = BounMinus.Ncp;
	double			LcpPlus = BounPlus.Lcp;
	double			LcpMinus = BounMinus.Lcp;
	CDoubleVector	velocity = CDoubleVector(0., 0.);


	if (VelocityBoundary == enumPlus || VelocityBoundary == enumMinus
		||CircleCenterBoundary == enumPlus || CircleCenterBoundary == enumMinus) { return velocity; }
	CDoubleMatrix MatrixPInverse;
	MatrixPInverse = GeometryAlg::MatrixInverse(MatrixP);

	double aa, bb, cc, dd, na, nb, ma, mb, ma1, mb1, PIStar, cos_theta, sin_theta;

	aa = MatrixPInverse.xx;
	bb = MatrixPInverse.xy;
	cc = MatrixPInverse.yx;
	dd = MatrixPInverse.yy;
	na = m_RHS.x;
	nb = m_RHS.y;
	ma = NcpPlus.x * LcpPlus;
	ma1 = NcpMinus.x * LcpMinus;
	mb = NcpPlus.y * LcpPlus;
	mb1 = NcpMinus.y * LcpMinus;
	cos_theta = NcpPlus.x;
	sin_theta = NcpPlus.y;

	bool IsSolved, IsColinear;
	IsSolved = false;

	
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.x = aa * (ma*ValPlus + ma1*ValMinus + na) +
				bb*(mb*ValPlus + mb1*ValMinus + nb);
			velocity.y = cc* (ma*ValPlus + ma1*ValMinus + na) +
				dd*(mb*ValPlus + mb1*ValMinus + nb);
			IsSolved = true;
		}
	}

	
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			IsColinear = false;
			if (fabs(NcpPlus.x * NcpMinus.y - NcpPlus.y * NcpMinus.x) < 1e-10) { IsColinear = true; }

			
			if (IsColinear)
			{
				
				if (fabs(NcpPlus.x) < 1e-12)
				{
					velocity.y = ValPlus;
					if (fabs(ma*cc + mb*dd) > 1e-12)
					{
						PIStar = (ValPlus - na*cc - nb*dd) / (ma*cc + mb*dd);
					}
					else
					{
						PIStar = 0.0;
					}
					velocity.x = aa*(ma*PIStar + na) + bb*(mb*PIStar + nb);
				}

				
				else if (fabs(NcpPlus.y) < 1e-12)
				{
					velocity.x = ValPlus;
					if (fabs(ma*aa + mb*bb) > 1e-12)
					{
						PIStar = (ValPlus - na*aa - nb*bb) / (ma*aa + mb*bb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.y = cc*(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
				else
				{
					
					
					if (fabs(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb) > 1e-12)
					{
						PIStar = (ValPlus - cos_theta*aa*na - cos_theta*bb*nb - sin_theta*cc*na - sin_theta*dd*nb) /
							(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.x = aa *(ma*PIStar + na) + bb*(mb*PIStar + nb);
					velocity.y = cc *(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
			}
			else
			{
				
				velocity = ValPlus*NcpPlus + ValMinus*NcpMinus;
			}
		}
		IsSolved = true;
	}

	
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			if (fabs(NcpMinus.x) < 1e-12)
			{
				velocity.y = ValMinus;
				PIStar = (velocity.y - cc*(na + ma*ValPlus) - dd*(nb + mb*ValPlus)) /
					(cc*ma1 + dd*mb1);
				velocity.x = aa*(na + ma1*PIStar + ma*ValPlus) +
					bb*(nb + mb1*PIStar + mb*ValPlus);
			}
			else if (fabs(NcpMinus.y) < 1e-12)
			{
				velocity.x = ValMinus;
				PIStar = (velocity.x - aa*(na + ma*ValPlus) - bb*(nb + mb*ValPlus)) /
					(aa*ma1 + bb*mb1);
				velocity.y = cc*(na + ma1*PIStar + ma*ValPlus) +
					dd*(nb + mb1*PIStar + mb*ValPlus);
			}
		}
		IsSolved = true;
	}

	
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			if (fabs(NcpPlus.x) < 1e-12)
			{
				velocity.y = ValPlus;
				PIStar = (velocity.y - cc*(na + ma1*ValMinus) - dd*(nb + mb1*ValMinus)) /
					(cc*ma + dd*mb);
				velocity.x = aa*(na + ma*PIStar + ma1*ValMinus) +
					bb*(nb + mb*PIStar + mb1*ValMinus);
			}
			else if (fabs(NcpPlus.y) < 1e-12)
			{
				velocity.x = ValPlus;
				PIStar = (velocity.x - aa*(na + ma1*ValMinus) - bb*(nb + mb1*ValMinus)) /
					(aa*ma + bb*mb);
				velocity.y = cc*(na + ma*PIStar + ma1*ValMinus) +
					dd*(nb + mb*PIStar + mb1*ValMinus);
			}
		}
		IsSolved = true;
	}

	
	if (enumPlus == SymmetryBoundary || enumMinus == SymmetryBoundary)
	{
		velocity.y = 0.;
		velocity.x = na / MatrixP.xx;
		IsSolved = true;
	}

	
	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValMinus * NcpMinus.x;
			IsSolved = true;
		}
	}

	
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValPlus * NcpPlus.x;
			IsSolved = true;
		}
	}

	
	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValPlus*ma) / MatrixP.xx;
			IsSolved = true;
		}
	}

	
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValMinus*ma1) / MatrixP.xx;
			IsSolved = true;
		}
	}

	if (!IsSolved)
	{
		printf("Boundary condition is not solved\n");
	}
	return velocity;
}


static void 
quadrant_hanging_point_matrix_assemble_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside, *m_quad_data_full;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face, parent_face_index;
	CDoubleMatrix	MatrixP = CDoubleMatrix(0., 0., 0., 0.);
	CDoubleVector	RHS = CDoubleVector(0., 0.);
	CPointBounInfo	OneBounPlus, OneBounMinus, BounParent;

	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);

		if (side[i]->is_hanging == Hanging)
		{
			int i_parent = 1 - i;
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);

			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}

			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_full = (p4est_quadrant *)side[full_index]->is.full.quad;
			parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_quad_data_full = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_quad_data_full = (quad_data_t *)quad_full->p.user_data;
			}
			ParentBounInfo	*PCInfo = (ParentBounInfo	*)&m_quad_data_full->m_pc_edge_data;

			CDoubleVector	master_coord[2];

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::LEFTUP);
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::RIGHTBOTTOM);
				master_coord[1] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::RIGHTUP);
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::LEFTBOTTOM);
				master_coord[1] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::RIGHTBOTTOM);
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::LEFTUP);
				master_coord[1] = m_quad_data_full->m_vara.corner_vector(idcnCoords_relaxed, quad_data_t::EnumCorner::RIGHTUP);
				break;
			}


			MatrixP = m_quad_data->m_vara.MarCnData[idcnMcp][m_which_corner[0]] +
				m_quad_data_aside->m_vara.MarCnData[idcnMcp][m_which_corner[1]] +
				m_quad_data_full->m_vara.MarCnData[ideMcp][parent_face_index];
			RHS = m_quad_data->m_vara.corner_vector(idcnRHS, m_which_corner[0]) +
				m_quad_data_aside->m_vara.corner_vector(idcnRHS, m_which_corner[1]) +
				m_quad_data_full->m_vara.corner_vector(ideRHS, parent_face_index);
			if (target_trace_enabled() && p4est_data->current_step == 3 &&
				((is_trace_fine(quad) && is_trace_sibling(quad_aside)) ||
				 (is_trace_sibling(quad) && is_trace_fine(quad_aside)))) {
				FILE *f = open_corner2_trace(info->p4est);
				if (f) {
					fprintf(f, "TRACE stage=HANGING_SUM iter=%d fine0=(%d,%d,L%d,c%d,g%d) fine1=(%d,%d,L%d,c%d,g%d) parent=(%d,%d,L%d,face%d,g%d)",
						g_trace_riemann_iter, quad->x, quad->y, quad->level, m_which_corner[0], side[i]->is.hanging.is_ghost[0] ? 1 : 0,
						quad_aside->x, quad_aside->y, quad_aside->level, m_which_corner[1], side[i]->is.hanging.is_ghost[1] ? 1 : 0,
						quad_full->x, quad_full->y, quad_full->level, parent_face_index, side[full_index]->is.full.is_ghost ? 1 : 0);
					trace_matrix(f, "fine0_M", m_quad_data->m_vara.MarCnData[idcnMcp][m_which_corner[0]]);
					trace_vector(f, "fine0_R", m_quad_data->m_vara.corner_vector(idcnRHS, m_which_corner[0]));
					trace_matrix(f, "fine1_M", m_quad_data_aside->m_vara.MarCnData[idcnMcp][m_which_corner[1]]);
					trace_vector(f, "fine1_R", m_quad_data_aside->m_vara.corner_vector(idcnRHS, m_which_corner[1]));
					trace_matrix(f, "parent_M", m_quad_data_full->m_vara.MarCnData[ideMcp][parent_face_index]);
					trace_vector(f, "parent_R", m_quad_data_full->m_vara.corner_vector(ideRHS, parent_face_index));
					trace_matrix(f, "sum_M", MatrixP);
					trace_vector(f, "sum_R", RHS);
					fprintf(f, "\n");
					fclose(f);
				}
			}
			if (!side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data->points[m_which_corner[0]].MatrixP = MatrixP;
				m_quad_data->points[m_which_corner[0]].RHS = RHS;
			}
			if (!side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside->points[m_which_corner[1]].MatrixP = MatrixP;
				m_quad_data_aside->points[m_which_corner[1]].RHS = RHS;
			}

			CDoubleVector		hanging_coord;
			hanging_coord = m_quad_data->m_vara.corner_vector(idcnCoords_half, m_which_corner[0]);

			OneBounPlus.Ncp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Ncp;
			OneBounPlus.Lcp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Lcp;
			OneBounPlus.delta_u_cp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].delta_u_cp;
			OneBounPlus.Uc_cur = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Uc_cur;
			OneBounPlus.Zc = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Zcp;
			OneBounPlus.enumType = WallBoundary;
			OneBounMinus.Ncp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Ncp;
			OneBounMinus.Lcp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Lcp;
			OneBounMinus.delta_u_cp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].delta_u_cp;
			OneBounMinus.Uc_cur = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Uc_cur;
			OneBounMinus.Zc = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Zcp;
			OneBounMinus.enumType = WallBoundary;
			BounParent.Ncp = --OneBounPlus.Ncp;
			BounParent.Lcp = OneBounPlus.Lcp + OneBounMinus.Lcp;
			BounParent.Uc_cur = m_quad_data_full->m_vara.corner_vector(idReconstructVelocity, 0);
			BounParent.delta_u_cp = OneBounPlus.delta_u_cp + OneBounPlus.Uc_cur - BounParent.Uc_cur;
			BounParent.Zc = m_quad_data_full->m_cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::plus].Zcp;

			BounParent.enumType = WallBoundary;

			if (!side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data->points[m_which_corner[0]].IsHanging = true;
				m_quad_data->points[m_which_corner[0]].TwoBouns[0] = OneBounPlus;
				m_quad_data->points[m_which_corner[0]].TwoBouns[1] = OneBounMinus;
				m_quad_data->points[m_which_corner[0]].BounParent = BounParent;
				m_quad_data->points[m_which_corner[0]].master_coord_relaxed[0] = master_coord[0];
				m_quad_data->points[m_which_corner[0]].master_coord_relaxed[1] = master_coord[1];
				m_quad_data->points[m_which_corner[0]].hanging_coord = hanging_coord;
			}

			if (!side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside->points[m_which_corner[1]].IsHanging = true;
				m_quad_data_aside->points[m_which_corner[1]].TwoBouns[0] = OneBounMinus;
				m_quad_data_aside->points[m_which_corner[1]].TwoBouns[1] = OneBounPlus;
				m_quad_data_aside->points[m_which_corner[1]].BounParent = BounParent;
				m_quad_data_aside->points[m_which_corner[1]].master_coord_relaxed[0] = master_coord[0];
				m_quad_data_aside->points[m_which_corner[1]].master_coord_relaxed[1] = master_coord[1];
				m_quad_data_aside->points[m_which_corner[1]].hanging_coord = hanging_coord;
			}
		}
	}
}

void MatrixAssemble(p4est_t *p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;


	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_corner_matrix_assemble_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

	if (!session.empty()) {
		session.exchange();
	}

	p4est_iterate(p4est,
		session.get(),
		(void*)session.data(),
		NULL,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		quadrant_corner_to_point_matrix_assemble_callback);
}

static void quadrant_copy_velocity_from_lag_to_relax_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	
	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->corner_vector(idcnVelocity_relaxed, k) = m_vara->corner_vector(idcnVelocity_lag, k);
	}
}

static void quadrant_update_parent_velo_press_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside, *m_quad_data_full;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face, parent_face_index;

	
	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	
	for (int i = 0; i < 2; i++)
	{
		if (sides->elem_count < 2 && i == 1)
		{
			continue;
		}
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_full = (p4est_quadrant *)side[full_index]->is.full.quad;
			parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_quad_data_full = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_quad_data_full = (quad_data_t *)quad_full->p.user_data;
			}
			ParentBounInfo	*PCInfo = (ParentBounInfo	*)&m_quad_data_full->m_pc_edge_data;
			PCInfo[parent_face_index].Hanging_velocity = m_quad_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[0]);

			if (m_quad_data->points[m_which_corner[0]].IsHanging == true &&
				m_quad_data->points[m_which_corner[0]].add_dissipation_parent == true)
			{
				PCInfo[parent_face_index].addDiss = true;
				PCInfo[parent_face_index].ParentPIStar = m_quad_data->points[m_which_corner[0]].pi_constrained_parent;
			}
		}
	}
}


void ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est_t *p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;


	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_compute_relaxed_info_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);


	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_parent_edge_matrix_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

	if (!session.empty()) {
		session.exchange();
	}

	p4est_iterate(p4est,
		session.get(),
		(void*) session.data(),
		NULL,
		quadrant_hanging_point_matrix_assemble_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);


	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		quadrant_relaxed_hanging_solver_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}


static void quadrant_corner_velocity_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_iter_corner_side_t	*side[CNDIM];  
	sc_array_t					*sides = &(info->sides);
	int							which_corner, cnid, is_ghost, m_size;
	int							quadid;
	int							tree_boundary;
	bool						is_boundary ;
	quad_data_t					*m_data;
	CVariable					*m_vara;
	quad_data_t					*ghost_data = (quad_data_t *)user_data;

	
	tree_boundary = info->tree_boundary;
	
	
	m_size = int(sides->elem_count);
	CHalf_edge_data *m_plus, *m_minus;
	is_boundary = false;
	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t *)side[i]->quad->p.user_data;
		}
		m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];
		if (m_plus->enumBYD != InnerBoundary) { is_boundary = true; }
		if (m_minus->enumBYD != InnerBoundary) { is_boundary = true; }
	}

	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t *)&ghost_data[quadid];
			m_vara = (CVariable *)&ghost_data[quadid].m_vara;
		}
		else
		{
			m_data = (quad_data_t *)side[i]->quad->p.user_data;
			m_vara = (CVariable *)&m_data->m_vara;
		}


		if (!is_ghost)
		{
			if (is_boundary)
			{


				m_data->points[cnid].velo_lag = BoundaryNodeVelocityComputation(
					m_data->points[cnid].TwoBouns[0],
					m_data->points[cnid].TwoBouns[1],
					m_data->points[cnid].MatrixP,
					m_data->points[cnid].RHS);
			}
			else
			{
				CDoubleMatrix MatrixP_Inverse;
				MatrixP_Inverse = GeometryAlg::MatrixInverse(m_data->points[cnid].MatrixP);
				m_data->points[cnid].velo_lag = GeometryAlg::MatrixDotVector(MatrixP_Inverse, m_data->points[cnid].RHS);
			}

			if (fabs(m_data->points[cnid].velo_lag.x) < m_eps) { m_data->points[cnid].velo_lag.x = 0.; }
			if (fabs(m_data->points[cnid].velo_lag.y) < m_eps) { m_data->points[cnid].velo_lag.y = 0.; }


			m_vara->corner_vector(idcnVelocity_lag, cnid) = m_data->points[cnid].velo_lag;
		}
		p4est_data_t *p4est_data = (p4est_data_t *)info->p4est->user_pointer;
		if (target_trace_enabled() && p4est_data->current_step == 3 && !is_ghost &&
			((is_trace_fine(side[i]->quad) && cnid == 2) ||
			 (is_trace_parent(side[i]->quad) && (cnid == 0 || cnid == 3)))) {
			FILE *f = open_corner2_trace(info->p4est);
			if (f) {
				fprintf(f, "TRACE stage=CORNER_SOLVE iter=%d cell=(%d,%d,L%d,c%d) hanging=%d", g_trace_riemann_iter,
					side[i]->quad->x, side[i]->quad->y, side[i]->quad->level, cnid, m_data->points[cnid].IsHanging ? 1 : 0);
				trace_matrix(f, "MatrixP", m_data->points[cnid].MatrixP);
				trace_vector(f, "RHS", m_data->points[cnid].RHS);
				trace_vector(f, "velo", m_data->points[cnid].velo_lag);
				fprintf(f, "\n");
				fclose(f);
			}
		}


	}
}


static void ComputeCornerNodeVelocity(p4est_t * p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	p4est_iterate(p4est,
		session.get(),
		(void*)session.data(),
		NULL,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		quadrant_corner_velocity_callback);

	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_copy_velocity_from_lag_to_relax_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}


static void quadrant_update_corner_coordinate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	double				delta_time = p4est_data->dt_iter;
	for (int k = 0; k < CNDIM; k++)  
	{
		m_vara->corner_vector(idcnCoords_lag, k) = m_vara->corner_vector(idcnCoords_half, k) +
			CDoubleVector(m_vara->corner_vector(idcnVelocity_lag, k).x * delta_time, m_vara->corner_vector(idcnVelocity_lag, k).y * delta_time);
	}
	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_lag, i); }

	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	m_vara->cell_vector(idCentroidCoord_lag) = center_point;

	for (int idIndex = idEChildrenCoordinate_lag; idIndex < idVectorEdgeVariableNum; idIndex++)
	{
		VectorCornerVariableID idcnVara;
		switch (idIndex)
		{
		case idEChildrenCoordinate_lag:
			idcnVara = idcnCoords_lag;
			break;
		case idEChildrenVelocity_lag:
			idcnVara = idcnVelocity_lag;
			break;
		default:
			break;
		}
		m_vara->edge_vector(static_cast<VectorEdgeVariableID>(idIndex), quad_data_t::EnumEdge::LEFT) = 0.5 *
			(m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTUP) +
				m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTBOTTOM));
		m_vara->edge_vector(static_cast<VectorEdgeVariableID>(idIndex), quad_data_t::EnumEdge::RIGHT) = 0.5 *
			(m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTUP) +
				m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTBOTTOM));
		m_vara->edge_vector(static_cast<VectorEdgeVariableID>(idIndex), quad_data_t::EnumEdge::BOTTOM) = 0.5 *
			(m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTBOTTOM) +
				m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTBOTTOM));
		m_vara->edge_vector(static_cast<VectorEdgeVariableID>(idIndex), quad_data_t::EnumEdge::UP) = 0.5 *
			(m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTUP) +
				m_vara->corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTUP));
	}
}


static void ComputeCoordinate(p4est_t * p4est)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_update_corner_coordinate_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_update_density_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	CDoubleVector		m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_lag, i); }

	m_vara->cell(idVolume) = GeometryAlg::CalculateCellVolume(coordinate_type, m_cell_coord);
	m_vara->cell(idDensity_lag) = m_vara->cell(idMass) / m_vara->cell(idVolume);


}


static void UpdateDensity(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_update_density_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_update_momentum_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	int					scheme_type = p4est_data->Scheme_type;
	CDoubleVector		SumFcp = CDoubleVector(0., 0.);
	CDoubleVector		center_point;
	double				m_alpha = 0.;
	if (coordinate_type == p4est_data_t::MyCoordType::cylinder
		&& scheme_type == p4est_data_t::MySchemeType::ControlVolume) {
		m_alpha = 1.;
	}
	CDoubleVector m_baser = CDoubleVector(1., 0.);
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		if (scheme_type == p4est_data_t::MySchemeType::ControlVolume) 
		{
			SumFcp += m_vara->corner_vector(idcnFcp, cnid) + m_vara->corner_vector(idcnFluxRelaxed, cnid);
		}
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
		{
			if (PCInfo[eind].IsParentChildBoun==true)
			{
				SumFcp += m_vara->corner_vector(ideFcp, eind) + PCInfo[eind].FluxRelaxed;
			}
		}
	}

	if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
	{
		m_vara->cell_vector(idCentroidVelo_lag) = m_vara->cell_vector(idCentroidVelo_half) -
			p4est_data->dt_iter * SumFcp / m_vara->cell(idMass);
	}
	else if (scheme_type == p4est_data_t::MySchemeType::AreaWeighted)
	{
		CDoubleVector m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_cur, i); }
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		m_vara->cell_vector(idCentroidVelo_lag) = m_vara->cell_vector(idCentroidVelo_half) -
			p4est_data->dt_iter * SumFcp / m_vara->cell(idMass) / center_point.x;
	}
}


static void UpdateMomentumEquation(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_update_momentum_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_compute_work_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	double				m_alpha = 1.;
	double				m_beta = 1.;

	
	m_vara->cell(idKineticVariation) = 0.;
	m_vara->cell(idTotalWork) = 0.;

	if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
	{
		m_alpha = 2.* M_PI;
		m_beta = 2. * M_PI * m_vara->cell_vector(idCentroidCoord_cur).y;
	}
	CDoubleVector Velo = 0.5 * (m_vara->cell_vector(idCentroidVelo_half) + m_vara->cell_vector(idCentroidVelo_lag));
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		
		if (coordinate_type == p4est_data_t::MyCoordType::plane)
		{
			
			m_vara->cell(idKineticVariation) += m_beta * 
				Velo^ (m_vara->corner_vector(idcnFcp, cnid)+ m_vara->corner_vector(idcnFluxRelaxed, cnid));
		}
		if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
		{
			
			m_vara->cell(idKineticVariation) += m_beta * Velo^ m_vara->corner_vector(idAWFcp, cnid);
		}

		
		m_vara->cell(idTotalWork) += m_alpha*
			m_vara->corner_vector(idcnVelocity_lag, cnid) ^ 
			(m_vara->corner_vector(idcnFcp, cnid)+ m_vara->corner_vector(idcnFluxRelaxed, cnid)); 
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		
		if (PCInfo[eind].IsParentChildBoun==true)
		{
			if (coordinate_type == p4est_data_t::MyCoordType::plane)
			{
				m_vara->cell(idKineticVariation) += m_beta * Velo ^
					(m_vara->corner_vector(ideFcp, eind) + PCInfo[eind].FluxRelaxed);
			}
			m_vara->cell(idTotalWork) += m_alpha* PCInfo[eind].Hanging_velocity ^
				(m_vara->corner_vector(ideFcp, eind) + PCInfo[eind].FluxRelaxed);
		}
	}
}


static void ComputeWork(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_compute_work_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_update_energy_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	
	m_vara->cell(idTotalEnergy_lag) = m_vara->cell(idTotalEnergy_half) - p4est_data->dt_iter * m_vara->cell(idTotalWork) / m_vara->cell(idMass);


	double source = 0.;
	if (p4est_data->which_case == ProblemNo::TaylorGreen)
	{
		source = p4est_data->dt_iter * 5.*M_PI / 8.*m_vara->cell(idVolume) *
			(cos(3.*M_PI*m_vara->cell_vector(idCentroidCoord_lag).x)*cos(M_PI * m_vara->cell_vector(idCentroidCoord_lag).y) -
				cos(M_PI*m_vara->cell_vector(idCentroidCoord_lag).x)*cos(3.*M_PI*m_vara->cell_vector(idCentroidCoord_lag).y)) / m_vara->cell(idMass);
	}

	if (m_vara->cell(idTotalEnergy_lag) > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}

	
	m_vara->cell(idInternalEnergy_lag) = m_vara->cell(idInternalEnergy_half) - p4est_data->dt_iter
		* (m_vara->cell(idTotalWork) - m_vara->cell(idKineticVariation)) / m_vara->cell(idMass);
	m_vara->cell(idInternalEnergy_lag) += source;
	if (m_vara->cell(idInternalEnergy_lag) > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}
}


static void UpdateEnergyEquation(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_update_energy_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_update_EOS_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idPressure_lag) = PhysicalAlg::EquationOfState(m_vara->cell(idGamma), m_vara->cell(idDensity_lag), m_vara->cell(idInternalEnergy_lag));
	if (m_vara->cell(idPressure_lag) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the value of pressure is illegal\n");
	}
}


static void UpdateEquationOfState(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_update_EOS_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void quadrant_accept_center_solution_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	m_vara->cell_vector(idCentroidCoord_cur) = m_vara->cell_vector(idCentroidCoord_lag);
	m_vara->cell_vector(idCentroidVelo_cur) = m_vara->cell_vector(idCentroidVelo_lag);
	m_vara->cell(idDensity_cur) = m_vara->cell(idDensity_lag);
	m_vara->cell(idTotalEnergy_cur) = m_vara->cell(idTotalEnergy_lag);
	m_vara->cell(idInternalEnergy_cur) = m_vara->cell(idInternalEnergy_lag);

	if (m_vara->cell(idInternalEnergy_cur) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}

	m_vara->cell(idPressure_cur) = m_vara->cell(idPressure_lag);

	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_vara->corner_vector(idcnCoords_cur, cnid) = m_vara->corner_vector(idcnCoords_lag, cnid);
		m_vara->corner_vector(idcnVelocity_cur, cnid) = m_vara->corner_vector(idcnVelocity_lag, cnid);
	}

	generate_children_info_from_parent(p4est_data, m_vara);
}


static void quadrant_total_energy_error_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	p4est_data->total_energy_lag += m_vara->cell(idMass) * m_vara->cell(idTotalEnergy_lag);
	p4est_data->total_energy_cur += m_vara->cell(idMass) * m_vara->cell(idTotalEnergy_cur);


}

static void
quadrant_reset_parent_edge_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*m_quad_data = (quad_data_t *)info->quad->p.user_data;

	for (int eind = 0; eind < CNDIM; eind++)
	{
		m_quad_data->m_pc_edge_data[eind].IsParentChildBoun = false;
		m_quad_data->m_pc_edge_data[eind].Lcp[0] = 0.;
		m_quad_data->m_pc_edge_data[eind].Lcp[1] = 0.;
		m_quad_data->m_pc_edge_data[eind].Ncp[0] = CDoubleVector(0., 0.);
		m_quad_data->m_pc_edge_data[eind].Ncp[1] = CDoubleVector(0., 0.);
		m_quad_data->m_pc_edge_data[eind].FluxRelaxed = CDoubleVector(0., 0.);
	}
}

static void quadrant_set_init_parent_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)user_data;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;
	CVariable		*m_child1_vara, *m_child2_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];
	

	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;
			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
			}
			CCorner_data		*cndata = (CCorner_data *)&m_parent_data->m_cndata;
			ParentBounInfo		*PCInfo = (ParentBounInfo  *)&m_parent_data->m_pc_edge_data;
			CHalf_edge_data *m_plus, *m_minus;

			if (!side[full_index]->is.full.is_ghost)
			{
				PCInfo[parent_face_index].IsParentChildBoun =
					m_child1_data->points[m_which_corner[0]].IsHanging;

				PCInfo[parent_face_index].ParentPIStar =

					m_child1_data->points[m_which_corner[0]].pi_constrained_parent;

				PCInfo[parent_face_index].Hanging_velocity =
					m_child1_data->m_vara.corner_vector(idcnVelocity_lag, m_which_corner[0]);

				PCInfo[parent_face_index].Lcp[0] =
					m_child1_data->points[m_which_corner[0]].TwoBouns[0].Lcp;

				PCInfo[parent_face_index].Lcp[1] =
					m_child2_data->points[m_which_corner[1]].TwoBouns[0].Lcp;

				PCInfo[parent_face_index].Ncp[0] = --m_child1_data->points[m_which_corner[0]].TwoBouns[0].Ncp;
				PCInfo[parent_face_index].Ncp[1] = --m_child2_data->points[m_which_corner[1]].TwoBouns[0].Ncp;
			}

			CDoubleVector	master_coord[2], hanging_coord;
			hanging_coord = m_child1_data->m_vara.corner_vector(idcnCoords_cur, m_which_corner[0]);

			if (!side[full_index]->is.full.is_ghost)
			{
				switch (parent_face_index)
				{
				case quad_data_t::EnumEdge::LEFT:
					m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTBOTTOM].hdata[CHalf_edge_data::cside::plus];
					m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::minus];
					master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
					master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);
					m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
					m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
					break;
				case quad_data_t::EnumEdge::RIGHT:
					m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTUP].hdata[CHalf_edge_data::cside::plus];
					m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTBOTTOM].hdata[CHalf_edge_data::cside::minus];
					master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);
					master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);
					m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
					m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
					break;
				case quad_data_t::EnumEdge::BOTTOM:
					m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTBOTTOM].hdata[CHalf_edge_data::cside::plus];
					m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTBOTTOM].hdata[CHalf_edge_data::cside::minus];
					master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTBOTTOM);
					master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTBOTTOM);
					m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
					m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
					break;
				case quad_data_t::EnumEdge::UP:
					m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::plus];
					m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTUP].hdata[CHalf_edge_data::cside::minus];
					master_coord[0] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::RIGHTUP);
					master_coord[1] = m_parent_data->m_vara.corner_vector(idcnCoords_cur, quad_data_t::EnumCorner::LEFTUP);
					m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
					m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
					break;
				default:
					break;
				}
			}
		}
	}
}

static void
quadrant_get_children_hanging_info_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	CPointBounInfo	OneBounPlus, OneBounMinus;

	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);

		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);

			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}

			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			OneBounPlus.Ncp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Ncp;
			OneBounPlus.Lcp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Lcp;
			OneBounPlus.delta_u_cp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].delta_u_cp;
			OneBounPlus.Uc_cur = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Uc_cur;
			OneBounPlus.Zc = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Zcp;
			OneBounPlus.enumType = WallBoundary;
			OneBounMinus.Ncp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Ncp;
			OneBounMinus.Lcp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Lcp;
			OneBounMinus.delta_u_cp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].delta_u_cp;
			OneBounMinus.Uc_cur = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Uc_cur;
			OneBounMinus.Zc = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Zcp;
			OneBounMinus.enumType = WallBoundary;

			if (!side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data->points[m_which_corner[0]].IsHanging = true;
				m_quad_data->points[m_which_corner[0]].TwoBouns[0] = OneBounPlus;
				m_quad_data->points[m_which_corner[0]].TwoBouns[1] = OneBounMinus;
			}

			if (!side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside->points[m_which_corner[1]].IsHanging = true;
				m_quad_data_aside->points[m_which_corner[1]].TwoBouns[0] = OneBounMinus;
				m_quad_data_aside->points[m_which_corner[1]].TwoBouns[1] = OneBounPlus;
			}
		}
	}

}

static void Get_AMR_BDY_info(p4est_t *p4est, GhostSession &session)
{
	p4est_iterate(p4est,
		session.get(),
		(void*)session.data(),
		NULL,
		quadrant_get_children_hanging_info_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);


	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_reset_parent_edge_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);


	p4est_iterate(p4est,
		session.get(),
		(void*)session.data(),
		NULL,
		quadrant_set_init_parent_edge_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}


static void AcceptNumericalSolution(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_accept_center_solution_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void StatTotalEnergyError(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_data->total_energy_cur = 0.;
	p4est_data->total_energy_lag = 0.;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_total_energy_error_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         

	double local_energy_cur = p4est_data->total_energy_cur;
	double local_energy_lag = p4est_data->total_energy_lag;
	sc_MPI_Allreduce(&local_energy_cur, &p4est_data->total_energy_cur, 1, sc_MPI_DOUBLE, sc_MPI_SUM, p4est->mpicomm);
	sc_MPI_Allreduce(&local_energy_lag, &p4est_data->total_energy_lag, 1, sc_MPI_DOUBLE, sc_MPI_SUM, p4est->mpicomm);

	if (p4est_data->current_step == 1)
	{
		p4est_data->total_energy_init = p4est_data->total_energy_cur;
	}

	if (p4est->mpirank == 0) {
		p4est_data->EnergyFile << blank << blank << p4est_data->current_time << blank << blank <<
			(p4est_data->total_energy_lag - p4est_data->total_energy_cur) /
			p4est_data->total_energy_cur << endl;
	}

	P4EST_GLOBAL_PRODUCTIONF("the total energy error is %#.16g\n", (p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init);
	if (abs((p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init) > 1e-6)
	{
		P4EST_GLOBAL_PRODUCTIONF("The total energy is not conservative after time step\n");
		abort();
	}
}

static void quadrant_compute_corner_force_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					Scheme_type = p4est_data->Scheme_type;
	CCorner_data		*cndata = (CCorner_data *)&data->m_cndata;
	CHalf_edge_data		*m_plus, *m_minus;
	

	for (int k = 0; k < CNDIM; k++)
	{
		CDoubleVector DeltaU[CNDIM], McpDeltaUc, AWMcpDeltaUc, LcpNcpPc;
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		m_vara->corner_vector(idReconstructVelocity, k) = m_vara->cell_vector(idCentroidVelo_cur);
		m_vara->corner(idReconstructDensity, k) = m_vara->cell(idDensity_cur);
		m_vara->corner(idReconstructPressure, k) = m_vara->cell(idPressure_cur);
		DeltaU[k] = m_vara->corner_vector(idcnVelocity_lag, k) - m_vara->corner_vector(idReconstructVelocity, k);
		LcpNcpPc = (m_plus->Rcp * m_plus->Lcp * m_plus->Ncp +
			m_minus->Rcp * m_minus->Lcp * m_minus->Ncp)*m_vara->corner(idReconstructPressure, k);
		McpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnMcp][k],DeltaU[k]);
		m_vara->corner_vector(idcnFcp, k) = (m_plus->Rcp * m_plus->Lcp * m_plus->Ncp +
			m_minus->Rcp * m_minus->Lcp * m_minus->Ncp)*
			m_vara->corner(idReconstructPressure, k) - McpDeltaUc;
		if (Scheme_type == p4est_data_t::MySchemeType::AreaWeighted)
		{
			AWMcpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnAWMcp][k],
				DeltaU[k]);
			m_vara->corner_vector(idAWFcp, k) = (m_plus->Lcp * m_plus->Ncp + m_minus->Lcp * m_minus->Ncp)*
				m_vara->corner(idReconstructPressure, k) - AWMcpDeltaUc;
		}
	}


	for (int eind = 0; eind < CNDIM; eind++)
	{
		CDoubleVector DeltaU, McpDeltaUc, LcpNcpPc;
		DeltaU = PCInfo[eind].Hanging_velocity
			- m_vara->corner_vector(idReconstructVelocity, eind);
		LcpNcpPc = (PCInfo[eind].Lcp[0] * PCInfo[eind].Ncp[0] + PCInfo[eind].Lcp[1] * PCInfo[eind].Ncp[1])*
			m_vara->corner(idReconstructPressure, eind);
		McpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[ideMcp][eind], DeltaU);
		m_vara->corner_vector(ideFcp, eind) = (PCInfo[eind].Lcp[0] * PCInfo[eind].Ncp[0] + PCInfo[eind].Lcp[1] * PCInfo[eind].Ncp[1])*
			m_vara->corner(idReconstructPressure, eind) - McpDeltaUc;
	}
}


static void ComputeCornerAndEdgeForce(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		quadrant_compute_corner_force_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}

static void quadrant_flux_relaxed_reset_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	CCorner_data		*cndata = (CCorner_data *)&data->m_cndata;


	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->corner_vector(idcnFluxRelaxed, k) = CDoubleVector(0.,0.);
	}
}

void FluxRelaxedResetZero(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	
	p4est_iterate(p4est,
		NULL,          
		NULL,   
		quadrant_flux_relaxed_reset_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}


static void RiemannSolver(p4est_t * p4est, GhostSession &session)
{

	FluxRelaxedResetZero(p4est);

	for (int iter_num = 0; iter_num < fixed_iter_num; iter_num++)
	{
		g_trace_riemann_iter = iter_num;

		MatrixAssemble(p4est, session);
		trace_target_snapshot(p4est, "AFTER_MATRIX");
		session.exchange();


		ComputeCornerNodeVelocity(p4est, session);
		trace_target_snapshot(p4est, "AFTER_CORNER_SOLVE");
		session.exchange();

		ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est, session);
		trace_target_snapshot(p4est, "AFTER_HANGING");

		p4est_data_t * p4est_data = (p4est_data_t *)p4est->user_pointer;
		if (p4est_data->current_step == 1) {
			//IOAlgorithm::p4est_debug_output_vtu(p4est, "output/debug_checkpoint", 0, iter_num);
		}

		session.exchange();
	}

	
	ComputeCornerAndEdgeForce(p4est);
}


static void StatGlobalFieldChecksum(p4est_t *p4est, const char* label) {
    double local_sums[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double c[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    
    p4est_tree_t *tree;
    p4est_quadrant_t *quad;
    sc_array_t *tquadrants;
    for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; ++t) {
        tree = p4est_tree_array_index (p4est->trees, t);
        tquadrants = &tree->quadrants;
        for (size_t i = 0; i < tquadrants->elem_count; ++i) {
            quad = p4est_quadrant_array_index (tquadrants, i);
            quad_data_t *data = (quad_data_t *)quad->p.user_data;
            
            double vals[5] = {
                data->m_vara.cell(idMass),
                data->m_vara.cell(idTotalEnergy_lag),
                data->m_vara.cell(idDensity_lag),
                data->m_vara.cell_vector(idCentroidVelo_lag).x + data->m_vara.cell_vector(idCentroidVelo_lag).y,
                data->m_vara.cell(idTotalWork)
            };
            
            for(int k=0; k<5; ++k) {
                double y = vals[k] - c[k];
                double t_val = local_sums[k] + y;
                c[k] = (t_val - local_sums[k]) - y;
                local_sums[k] = t_val;
            }
        }
    }
    
    double global_sums[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    sc_MPI_Reduce(local_sums, global_sums, 5, sc_MPI_DOUBLE, sc_MPI_SUM, 0, p4est->mpicomm);
    
    if (p4est->mpirank == 0) {
        P4EST_GLOBAL_PRODUCTIONF("Checksum [%s]: Mass = %.14e, E = %.14e, Rho = %.14e, V = %.14e, W = %.14e\n", 
            label, global_sums[0], global_sums[1], global_sums[2], global_sums[3], global_sums[4]);
    }
}

static void advance_single_stage(p4est_t * p4est, GhostSession &session)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;


	get_boundary_from_p4est(p4est);
	p4est_data->dt_iter =
		StagePolicy::timestep_scale(0) * p4est_data->delta_time;

	CalculateHalfTimeVariable(p4est);
		trace_target_snapshot(p4est, "AFTER_HALF");
		//StatGlobalFieldChecksum(p4est, "Checkpoint 3: Predict");


		CalculateCornerRcpLcpNcp(p4est);
		trace_target_snapshot(p4est, "AFTER_RCP");
		session.exchange();


		Get_AMR_BDY_info(p4est, session);
		trace_target_snapshot(p4est, "AFTER_AMR_BDY");
		session.exchange();


		const SolverGate::CoordinateType coordinate_type =
			SolverGate::coordinate_type_from_legacy(p4est_data->coord_type);
		const SolverGate::SolverType solver_type =
			SolverGate::solver_type_from_legacy(p4est_data->solver_type);
		if (SolverGate::should_run_riemann(coordinate_type, solver_type))
		{
			RiemannSolver(p4est, session);
		}
		
		// Debug step 3 after RiemannSolver
		if (target_trace_enabled() && p4est_data->current_step == 3) {
			auto dbg_cb = [](p4est_iter_volume_info_t *info, void *user_data) {
				quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
				CVariable *m_vara = &data->m_vara;
				if (info->p4est->mpisize == 1 && info->quadid == 397) {
					char fname[256];
					sprintf(fname, "riemann_dbg_%d.txt", info->p4est->mpisize);
					FILE* f = fopen(fname, "a");
					if (f) {
						fprintf(f, "SERIAL 397 (x=%d, y=%d) corner velocities:\n", info->quad->x, info->quad->y);
						for (int j = 0; j < P4EST_CHILDREN; j++) {
							fprintf(f, "  Corner %d: vx=%f, vy=%f\n", j, 
								m_vara->corner_vector(idcnVelocity_cur, j).x, 
								m_vara->corner_vector(idcnVelocity_cur, j).y);
						}
						fclose(f);
					}
				}
				// In parallel, we don't know quadid. We match by x and y of the serial 397!
				if (info->p4est->mpisize > 1 && info->quad->x == 134217728 && info->quad->y == 528482304) {
					char fname[256];
					sprintf(fname, "riemann_dbg_%d.txt", info->p4est->mpisize);
					FILE* f = fopen(fname, "a");
					if (f) {
						fprintf(f, "PARALLEL MATCH (x=%d, y=%d) corner velocities:\n", info->quad->x, info->quad->y);
						for (int j = 0; j < P4EST_CHILDREN; j++) {
							fprintf(f, "  Corner %d: vx=%f, vy=%f\n", j, 
								m_vara->corner_vector(idcnVelocity_cur, j).x, 
								m_vara->corner_vector(idcnVelocity_cur, j).y);
						}
						fclose(f);
					}
				}
			};
			p4est_iterate(p4est, session.get(), session.data(), dbg_cb, NULL, NULL);
		}
		
		//StatGlobalFieldChecksum(p4est, "Checkpoint 4: RiemannSolver");

		
		ComputeDivergence(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 3: Divergence");

		
		ComputeCoordinate(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 4: Coordinate");

		
		UpdateDensity(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 5: Density");

		
		UpdateMomentumEquation(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 6: Momentum");

		
		ComputeWork(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 7: Work");

		
		UpdateEnergyEquation(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 8: EnergyEq");

		
		UpdateEquationOfState(p4est);
		if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 9: EOS");

		
	ComputeSoundSpeed(p4est);
	if (checksum_trace_enabled()) StatGlobalFieldChecksum(p4est, "SubStep 10: SoundSpeed");
	//StatGlobalFieldChecksum(p4est, "Checkpoint 5: Update");
	p4est_data->used_dt = p4est_data->delta_time;
}


static void quadrant_copy_coordx_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	sc_array_t		*array_data = (sc_array_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset;
	CCorner_data	*cndata = (CCorner_data *)&quad_data->m_cndata;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;
	arrayoffset = P4EST_CHILDREN * local_id;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double  *this_ptr = (double *)sc_array_index(array_data, arrayoffset + index0);
		this_ptr[0] = m_vara->corner_vector(idcnCoords_lag, i).x;
	}
}

static void quadrant_copy_cell_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	vtu_cell_data_t	*m_cell_data = (vtu_cell_data_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset, corner_arrayoffset;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;

	arrayoffset = local_id;
	corner_arrayoffset = CNDIM*local_id;
	double		*p_val = (double *)sc_array_index(m_cell_data->pressure_array, arrayoffset);
	double		*t_val = (double *)sc_array_index(m_cell_data->temperature_array, arrayoffset);
	double		*rho_val = (double *)sc_array_index(m_cell_data->density_array, arrayoffset);
	double		*ie_val = (double *)sc_array_index(m_cell_data->internal_energy_array, arrayoffset);

	*p_val = m_vara->cell(idPressure_lag);
	*t_val = 0.0;
	*rho_val = m_vara->cell(idDensity_lag);
	*ie_val = m_vara->cell(idInternalEnergy_lag);
}

static void quadrant_copy_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	vtu_cell_data_t	*m_cell_data = (vtu_cell_data_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset, corner_arrayoffset;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;

	arrayoffset = local_id;
	corner_arrayoffset = CNDIM * local_id;
	double		*p_val = (double *)sc_array_index(m_cell_data->pressure_array, arrayoffset);
	double		*t_val = (double *)sc_array_index(m_cell_data->temperature_array, arrayoffset);
	double		*rho_val = (double *)sc_array_index(m_cell_data->density_array, arrayoffset);
	double		*ie_val = (double *)sc_array_index(m_cell_data->internal_energy_array, arrayoffset);

	*p_val = m_vara->cell(idPressure_lag);
	*t_val = 0.0;
	*rho_val = m_vara->cell(idDensity_lag);
	*ie_val = m_vara->cell(idInternalEnergy_lag);
	for (int i = 0; i < CNDIM; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double *coordx_val = (double *)sc_array_index(m_cell_data->coordx, corner_arrayoffset + index0);
		coordx_val[0] = m_vara->corner_vector(idcnCoords_lag, i).x;

		double *coordy_val = (double *)sc_array_index(m_cell_data->coordy, corner_arrayoffset + index0);
		coordy_val[0] = m_vara->corner_vector(idcnCoords_lag, i).y;

		double *velox_val = (double *)sc_array_index(m_cell_data->velox, corner_arrayoffset + index0);
		velox_val[0] = m_vara->corner_vector(idcnVelocity_lag, i).x;

		double *veloy_val = (double *)sc_array_index(m_cell_data->veloy, corner_arrayoffset + index0);
		veloy_val[0] = m_vara->corner_vector(idcnVelocity_lag, i).y;
	}
}


static void
GetRefineCornerCoords(const CDoubleVector &coordLB,
	const CDoubleVector &coordLU,
	const CDoubleVector &coordRU,
	const CDoubleVector &coordRB,
	CDoubleVector children_coord[P4EST_CHILDREN*CNDIM])
{
	enum edgeEnum { LEFT, UP, RIGHT, BOTTOM };
	CDoubleVector	coord[CNDIM];
	coord[quad_data_t::EnumCorner::LEFTBOTTOM] = coordLB;
	coord[quad_data_t::EnumCorner::LEFTUP] = coordLU;
	coord[quad_data_t::EnumCorner::RIGHTUP] = coordRU;
	coord[quad_data_t::EnumCorner::RIGHTBOTTOM] = coordRB;

	CDoubleVector	EdgeMiddle[CNDIM], AverCentroid;
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		EdgeMiddle[k] = 0.5*(coord[k] + coord[knext]);
	}
	AverCentroid = GeometryAlg::GetPolyCenterByAverage(coord);

	
	children_coord[quad_data_t::EnumCorner::LEFTBOTTOM] = coord[quad_data_t::EnumCorner::LEFTBOTTOM];
	children_coord[quad_data_t::EnumCorner::LEFTUP] = EdgeMiddle[edgeEnum::LEFT];
	children_coord[quad_data_t::EnumCorner::RIGHTUP] = AverCentroid;
	children_coord[quad_data_t::EnumCorner::RIGHTBOTTOM] = EdgeMiddle[edgeEnum::BOTTOM];

	
	children_coord[CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = EdgeMiddle[edgeEnum::BOTTOM];
	children_coord[CNDIM + quad_data_t::EnumCorner::LEFTUP] = AverCentroid;
	children_coord[CNDIM + quad_data_t::EnumCorner::RIGHTUP] = EdgeMiddle[edgeEnum::RIGHT];
	children_coord[CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = coord[quad_data_t::EnumCorner::RIGHTBOTTOM];

	
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = EdgeMiddle[edgeEnum::LEFT];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::LEFTUP] = coord[quad_data_t::EnumCorner::LEFTUP];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::RIGHTUP] = EdgeMiddle[edgeEnum::UP];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = AverCentroid;

	
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = AverCentroid;
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::LEFTUP] = EdgeMiddle[edgeEnum::UP];
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::RIGHTUP] = coord[quad_data_t::EnumCorner::RIGHTUP];
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = EdgeMiddle[edgeEnum::RIGHT];
	return;
}

static void
GetRefineCornerVelos(const CDoubleVector velo[CNDIM], CDoubleVector children_velo[P4EST_CHILDREN*CNDIM])
{
	
	return;
}


static void
Lagrangian_replace_quads(p4est_t * p4est, p4est_topidx_t which_tree,
	int num_outgoing,
	p4est_quadrant_t *outgoing[],
	int num_incoming,
	p4est_quadrant_t *incoming[])
{
	enum edgeEnum { LEFT, UP, RIGHT, BOTTOM };
	enum m_geometry_id {m_coord, m_velo};
	enum m_physical_id {m_density, m_internal_energy};
	enum m_which_child {child1, child2, child3, child4};
	quad_data_t			*parent_data, *child_data, *child_data1, *child_data2, *child_data3, *child_data4;
	CVariable			*child_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)p4est->user_pointer;

	if (num_outgoing > 1)
	{
		

		parent_data = (quad_data_t *)incoming[0]->p.user_data;
		child_data1 = (quad_data_t *)outgoing[0]->p.user_data;
		child_data2 = (quad_data_t *)outgoing[1]->p.user_data;
		child_data3 = (quad_data_t *)outgoing[2]->p.user_data;
		child_data4 = (quad_data_t *)outgoing[3]->p.user_data;

		
		parent_data->m_vara.int_cell(idCoarseningTag) = p4est_data_t::CoarseningEnum::CoarsenedJustNow;

		
		for (int idIndex = idcnCoords_cur; idIndex <= idcnVelocity_lag; idIndex++)
		{
			parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTBOTTOM) =
				child_data1->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTBOTTOM);
			parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTUP) =
				child_data3->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTUP);
			parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTUP) =
				child_data4->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTUP);
			parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTBOTTOM) =
				child_data2->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTBOTTOM);
		}

		// idChildIndex is typed per-loop scope below: VectorCornerVariableID
		// in the geometry loop, DoubleCellVariableID in the physics loop.

		
		for (int idIndex = m_geometry_id::m_coord; idIndex <= m_geometry_id::m_velo; idIndex++)
		{
			VectorCornerVariableID idChildIndex;
			switch (idIndex)
			{
			case m_geometry_id::m_coord:
				idChildIndex = idcnCoords_lag;
				break;
			case m_geometry_id::m_velo:
				idChildIndex = idcnVelocity_lag;
				break;
			default:
				break;
			}

			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child1][cnid] =
					child_data1->m_vara.corner_vector(idChildIndex, cnid);
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child2][cnid] =
					child_data2->m_vara.corner_vector(idChildIndex, cnid);
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child3][cnid] =
					child_data3->m_vara.corner_vector(idChildIndex, cnid);
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child4][cnid] =
					child_data4->m_vara.corner_vector(idChildIndex, cnid);
			}
		}

		
		for (int idIndex = m_physical_id::m_density; idIndex <= m_physical_id::m_internal_energy; idIndex++)
		{
			DoubleCellVariableID idChildIndex;
			switch (idIndex)
			{
			case m_physical_id::m_density:
				idChildIndex = idDensity_lag;
				break;
			case m_physical_id::m_internal_energy:
				idChildIndex = idInternalEnergy_lag;
				break;
			default:
				break;
			}

			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child1] =
				child_data1->m_vara.cell(idChildIndex);
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child2] =
				child_data2->m_vara.cell(idChildIndex);
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child3] =
				child_data3->m_vara.cell(idChildIndex);
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child4] =
				child_data4->m_vara.cell(idChildIndex);
			if (parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child1] > m_eps &&
				parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child2] > m_eps &&
				parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child3] > m_eps &&
				parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child4] > m_eps)
			{
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("The value of ChildrenPhysicalVara is illegal in refining!\n");
				abort();
			}
		}

		parent_data->m_vara.cell(idMass) = child_data1->m_vara.cell(idMass) +
			child_data2->m_vara.cell(idMass) +
			child_data3->m_vara.cell(idMass) +
			child_data4->m_vara.cell(idMass); 

		
		CDoubleVector m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = parent_data->m_vara.corner_vector(idcnCoords_cur, i); }
		parent_data->m_vara.cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
		parent_data->m_vara.cell(idDensity_cur) = parent_data->m_vara.cell(idMass) / parent_data->m_vara.cell(idVolume);
		parent_data->m_vara.cell(idDensity_lag) = parent_data->m_vara.cell(idDensity_cur);
		CDoubleVector center_point;
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		parent_data->m_vara.cell_vector(idCentroidCoord_cur) = center_point;

		
		parent_data->m_vara.cell_vector(idCentroidVelo_cur) = (child_data1->m_vara.cell(idMass) * child_data1->m_vara.cell_vector(idCentroidVelo_cur) +
			child_data2->m_vara.cell(idMass) * child_data2->m_vara.cell_vector(idCentroidVelo_cur) +
			child_data3->m_vara.cell(idMass) * child_data3->m_vara.cell_vector(idCentroidVelo_cur) +
			child_data4->m_vara.cell(idMass) * child_data4->m_vara.cell_vector(idCentroidVelo_cur))
			/ parent_data->m_vara.cell(idMass);
		parent_data->m_vara.cell_vector(idCentroidVelo_lag) = parent_data->m_vara.cell_vector(idCentroidVelo_cur);

		parent_data->m_vara.cell(idGamma) = (
			child_data1->m_vara.cell(idGamma) +
			child_data2->m_vara.cell(idGamma) +
			child_data3->m_vara.cell(idGamma) +
			child_data4->m_vara.cell(idGamma)) / P4EST_CHILDREN;

		
		parent_data->m_vara.cell(idTotalEnergy_cur) = (
			child_data1->m_vara.cell(idMass) * child_data1->m_vara.cell(idTotalEnergy_cur) +
			child_data2->m_vara.cell(idMass) * child_data2->m_vara.cell(idTotalEnergy_cur) +
			child_data3->m_vara.cell(idMass) * child_data3->m_vara.cell(idTotalEnergy_cur) +
			child_data4->m_vara.cell(idMass) * child_data4->m_vara.cell(idTotalEnergy_cur))
			/ parent_data->m_vara.cell(idMass);
		parent_data->m_vara.cell(idTotalEnergy_lag) = parent_data->m_vara.cell(idTotalEnergy_cur);

		
		parent_data->m_vara.cell(idInternalEnergy_cur) = parent_data->m_vara.cell(idTotalEnergy_cur) -
			0.5 * (pow(parent_data->m_vara.cell_vector(idCentroidVelo_cur).x, 2) + pow(parent_data->m_vara.cell_vector(idCentroidVelo_cur).y, 2));
		if (parent_data->m_vara.cell(idInternalEnergy_cur) > m_eps)
		{

		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("The value of internal energy is illegal in refining!\n");
			abort();
		}
		parent_data->m_vara.cell(idInternalEnergy_lag) = parent_data->m_vara.cell(idTotalEnergy_lag) -
			0.5 * (pow(parent_data->m_vara.cell_vector(idCentroidVelo_lag).x, 2) + pow(parent_data->m_vara.cell_vector(idCentroidVelo_lag).y, 2));
		parent_data->m_vara.cell(idInternalEnergy_lag) = parent_data->m_vara.cell(idInternalEnergy_cur);

		
		parent_data->m_vara.cell(idPressure_lag) = PhysicalAlg::EquationOfState(
			parent_data->m_vara.cell(idGamma),
			parent_data->m_vara.cell(idDensity_lag),
			parent_data->m_vara.cell(idInternalEnergy_lag));
		parent_data->m_vara.cell(idPressure_cur) = parent_data->m_vara.cell(idPressure_lag);
		parent_data->m_vara.cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
			parent_data->m_vara.cell(idGamma),
			parent_data->m_vara.cell(idPressure_cur),
			parent_data->m_vara.cell(idDensity_cur));
	}
	else
	{
		

		CDoubleVector children_coord[P4EST_CHILDREN][CNDIM];

		parent_data = (quad_data_t *)outgoing[0]->p.user_data;

		double children_total_energy = 0.;
		double children_energy_per_mass = 0.;
		double children_total_mass = 0.;
		for (int i = 0; i < P4EST_CHILDREN; i++)
		{
			child_data = (quad_data_t *)incoming[i]->p.user_data;

			p4est_qcoord_t qx = incoming[i]->x;
			p4est_qcoord_t qy = incoming[i]->y;

			if (target_trace_enabled() && p4est_data->current_step == 3 && is_trace_fine(incoming[i])) {
				FILE *f = open_corner2_trace(p4est);
				if (f) {
					fprintf(f, "TRACE stage=REFINE_TRANSFER child_index=%d parent=(%d,%d,L%d) child=(%d,%d,L%d)", i,
						outgoing[0]->x, outgoing[0]->y, outgoing[0]->level, incoming[i]->x, incoming[i]->y, incoming[i]->level);
					for (int c = 0; c < CNDIM; ++c) {
						char name[64];
						sprintf(name, "parent_lag%d", c); trace_vector(f, name, parent_data->m_vara.corner_vector(idcnVelocity_lag, c));
						sprintf(name, "buffer_lag%d", c); trace_vector(f, name, parent_data->m_vara.ChildrenCnGeomVara[m_geometry_id::m_velo][i][c]);
					}
					fprintf(f, " parent_rho=%.17e buffer_rho=%.17e parent_ie=%.17e buffer_ie=%.17e\n",
						parent_data->m_vara.cell(idDensity_lag), parent_data->m_vara.ChildrenPhysicalVara[m_physical_id::m_density][i],
						parent_data->m_vara.cell(idInternalEnergy_lag), parent_data->m_vara.ChildrenPhysicalVara[m_physical_id::m_internal_energy][i]);
					fclose(f);
				}
			}

			FILE *f_dbg = NULL;
			double px = 0.;
			double py = 0.;
			if (refine_trace_enabled()) {
				px = parent_data->m_vara.cell_vector(idCentroidCoord_cur).x;
				py = parent_data->m_vara.cell_vector(idCentroidCoord_cur).y;
				char fname[256];
				sprintf(fname, "refine_dbg_%d_%d.txt", p4est->mpisize, p4est->mpirank);
				f_dbg = fopen(fname, "a");
				if (f_dbg) {
					fprintf(f_dbg, "REFINE_STEP_%d_PARENT at (%.6f, %.6f): parent SoundSpeed=%e, mass=%e, vol=%e\n",
						p4est_data->current_step, px, py, parent_data->m_vara.cell(idSoundSpeed), parent_data->m_vara.cell(idMass), parent_data->m_vara.cell(idVolume));
				}
			}
			
			for (int j = 0; j < idDoubleCellVariableNum; j++)
			{
				
				child_data->m_vara.cell(static_cast<DoubleCellVariableID>(j)) = parent_data->m_vara.cell(static_cast<DoubleCellVariableID>(j));
				if (j == idSoundSpeed && f_dbg) {
					fprintf(f_dbg, "REFINE_STEP_%d_CHILD at (%.6f, %.6f): child SoundSpeed=%e\n",
						p4est_data->current_step, px, py, child_data->m_vara.cell(idSoundSpeed));
				}
				if (parent_data->m_vara.cell(idInternalEnergy_cur) > m_eps)
				{
				}
				else
				{
					P4EST_GLOBAL_PRODUCTIONF("The cihldren internal energy is illegal in refining!\n");
					abort();
				}
			}
			if (f_dbg) {
				fclose(f_dbg);
			}
			for (int j = idReconstructPressure; j < idDoubleCornerVariableNum; j++)
			{
				for (int k = 0; k < CNDIM; k++)
				{
					
					child_data->m_vara.corner(static_cast<DoubleCornerVariableID>(j), k) = parent_data->m_vara.corner(static_cast<DoubleCornerVariableID>(j), k);
				}
			}
			for (int j = 0; j < idIntCellVariableNum; j++)
			{
				child_data->m_vara.int_cell(static_cast<IntCellVariableID>(j)) = parent_data->m_vara.int_cell(static_cast<IntCellVariableID>(j));
			}
			for (int j = 0; j < idVectorCellVariableNum; j++)
			{
				
				child_data->m_vara.cell_vector(static_cast<VectorCellVariableID>(j)) = parent_data->m_vara.cell_vector(static_cast<VectorCellVariableID>(j));
			}
			for (int j = 0; j < idVectorCornerVariableNum; j++)
			{
				for (int k = 0; k < CNDIM; k++)
				{
					
					child_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(j), k) = parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(j), k);
				}
			}

			
			int idParentGeometry;
			for (int idChildrenIndex = idcnCoords_cur; idChildrenIndex <= idcnVelocity_lag; idChildrenIndex++)
			{
				if (idChildrenIndex == idcnCoords_cur || idChildrenIndex == idcnCoords_lag || idChildrenIndex == idcnCoords_half)
				{
					idParentGeometry = m_geometry_id::m_coord;
				}
				if (idChildrenIndex == idcnVelocity_cur || idChildrenIndex == idcnVelocity_lag)
				{
					idParentGeometry = m_geometry_id::m_velo;
				}

				for (int cnid = 0; cnid < CNDIM; cnid++)
				{
					child_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idChildrenIndex), cnid) =
						parent_data->m_vara.ChildrenCnGeomVara[idParentGeometry][i][cnid];

					if (idChildrenIndex == idcnCoords_lag)
					{
						children_coord[i][cnid] = parent_data->m_vara.ChildrenCnGeomVara[idParentGeometry][i][cnid];
					}
				}
			}

			
			int idParentPhysical;
			for (int idChildrenIndex = idDensity_cur; idChildrenIndex <= idInternalEnergy_lag; idChildrenIndex++)
			{
				if (idChildrenIndex == idDensity_cur || idChildrenIndex == idDensity_half || idChildrenIndex == idDensity_lag)
				{
					idParentPhysical = m_physical_id::m_density;
				}
				if (idChildrenIndex == idInternalEnergy_cur || idChildrenIndex == idInternalEnergy_half || idChildrenIndex == idInternalEnergy_lag)
				{
					idParentPhysical = m_physical_id::m_internal_energy;
				}

				child_data->m_vara.cell(static_cast<DoubleCellVariableID>(idChildrenIndex)) =
					parent_data->m_vara.ChildrenPhysicalVara[idParentPhysical][i];
				double m_value = parent_data->m_vara.ChildrenPhysicalVara[idParentPhysical][i];
				if (child_data->m_vara.cell(static_cast<DoubleCellVariableID>(idChildrenIndex)) > m_eps)
				{
				}
				else
				{
					P4EST_GLOBAL_PRODUCTIONF("The cihldren value of idChildrenIndex is illegal in refining!\n");
					abort();
				}
			}
			
			for (int idVCn = idcnCoords_cur; idVCn <= idcnCoords_lag; idVCn++)
			{
				VectorCellVariableID idVC;
				switch (idVCn)
				{
				case idcnCoords_cur:
					idVC = idCentroidCoord_cur;
					break;
				case idcnCoords_half:
					idVC = idCentroidCoord_half;
					break;
				case idcnCoords_lag:
					idVC = idCentroidCoord_lag;
					break;
				default:
					break;
				}
				CDoubleVector m_cell_coord[CNDIM];
				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = child_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idVCn), i); }
				CDoubleVector center_point;
				center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
				child_data->m_vara.cell_vector(idVC) = center_point;

				if (idVCn == idcnCoords_cur)
				{
					child_data->m_vara.cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
					child_data->m_vara.cell(idMass) = PhysicalAlg::CalculateCellMass(
						child_data->m_vara.cell(idVolume), child_data->m_vara.cell(idDensity_cur));
				}
			}
			children_total_energy += child_data->m_vara.cell(idMass) * child_data->m_vara.cell(idTotalEnergy_lag);
			children_total_mass += child_data->m_vara.cell(idMass);
			children_energy_per_mass += child_data->m_vara.cell(idTotalEnergy_lag);

			child_vara = (CVariable *)&child_data->m_vara;
			generate_children_info_from_parent(p4est_data, child_vara);
		}

		double parent_total_energy = parent_data->m_vara.cell(idMass) * parent_data->m_vara.cell(idTotalEnergy_lag);
		double parent_energy_per_mass = parent_data->m_vara.cell(idTotalEnergy_lag);
		double parent_total_mass = parent_data->m_vara.cell(idMass);
		if (abs((parent_total_energy - children_total_energy)/ parent_total_energy) > 1e-10)
		{
			P4EST_GLOBAL_PRODUCTIONF("The total energy is not conservative during refining!\n");
			if (abs((parent_total_mass - children_total_mass) / parent_total_mass) > 1e-10)
			{
				P4EST_GLOBAL_PRODUCTIONF("In the mean time, the total mass is not conservative during refining!\n");
				P4EST_GLOBAL_PRODUCTIONF("error is %.10lf\n", (parent_total_mass - children_total_mass) / parent_total_mass);
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("However, the total mass is conservative during refining!\n");
			}

			if (abs((parent_energy_per_mass - children_energy_per_mass) / parent_energy_per_mass) > 1e-10)
			{
				P4EST_GLOBAL_PRODUCTIONF("In the mean time, the energy per mass is not conservative during refining!\n");
				P4EST_GLOBAL_PRODUCTIONF("error is %.10lf\n", (parent_energy_per_mass - children_energy_per_mass) / parent_energy_per_mass);
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("However, the energy per mass is conservative during refining!\n");
			}

		}
	}
	return;
}

static void quadrant_copy_coordy_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	sc_array_t		*array_data = (sc_array_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset;
	CCorner_data	*cndata = (CCorner_data *)&quad_data->m_cndata;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;
	arrayoffset = P4EST_CHILDREN * local_id;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double  *this_ptr = (double *)sc_array_index(array_data, arrayoffset + index0);
		this_ptr[0] = m_vara->corner_vector(idcnCoords_lag, i).y;
	}
}


static void
quadrant_set_default_coarsening_tag_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	
	
	data->m_vara.int_cell(idCoarseningTag) = p4est_data_t::CoarseningEnum::NotCoarsenedJustNow;

	
	data->m_vara.int_cell(idAllowCoarsening) = p4est_data_t::CoarseningEnum::CoarsingAllowed;
}

static void set_default_coarsening_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_set_default_coarsening_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}


static void
quadrant_set_default_refining_tag_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;

	CDoubleVector m_coord[CNDIM];
	
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_coord[cnid] = m_vara->corner_vector(idcnCoords_lag, CNDIM - 1 - cnid);

	}
	int IsConcaveQuad = GeometryAlg::is_concave_quad(m_coord);
	m_vara->int_cell(idAllowRefining) = IsConcaveQuad;

	if (IsConcaveQuad < 0)
	{
		
		data->m_vara.int_cell(idAllowRefining) = p4est_data_t::RefiningEnum::RefiningAllowed;
	}
	else
	{
		

	}
}


static void
quadrant_predict_refining_quads_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	DoubleCellVariableID idCPara;
	p4est_qcoord_t		qx = info->quad->x;
	p4est_qcoord_t		qy = info->quad->y;
	int					level = info->quad->level;
	p4est_qcoord_t		length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	if (level < p4est_data->minus_level)
	{
		m_vara->int_cell(idAllowRefining) = p4est_data_t::RefiningEnum::MustRefing;
	}
	if (level >= p4est_data->max_level)
	{
		m_vara->int_cell(idAllowRefining) = p4est_data_t::RefiningEnum::RefiningNotAllowed;
	}

	if (m_vara->cell(idCPara) > p4est_data->refine_err)
	{
		m_vara->int_cell(idAllowRefining) = p4est_data_t::RefiningEnum::MustRefing;
	}
	else
	{
		m_vara->int_cell(idAllowRefining) = p4est_data_t::RefiningEnum::RefiningNotAllowed;
	}
}

static void 
set_default_refining_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_set_default_refining_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void
set_allowing_coarsening_tag(p4est_t *p4est, GhostSession &session)
{
	p4est_iterate(p4est,
		session.get(),
		session.data(),
		NULL,
		quadrant_whether_allowing_coarsening_from_edge_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		session.get(),
		session.data(),
		NULL,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		quadrant_whether_allowing_coarsening_from_corner_callback);
}

static void
Predict_refining_Quads(p4est_t *p4est, GhostSession &session)
{
	(void)session;
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_predict_refining_quads_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void
quadrant_reset_hanging_info_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;

	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		data->points[cnid].IsHanging = false;
		data->points[cnid].AddDiss = false;
		data->points[cnid].add_dissipation_parent = false;
	}

	for (int enid = 0; enid < CNDIM; enid++)
	{
		data->m_pc_edge_data[enid].addDiss = false;
	}
}


static void
quadrant_set_gradient_zero_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t*)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_t			*p4est = info->p4est;

	DoubleCellVariableID idCPara;
	DoubleEdgeVariableID idEPara;
	DoubleCornerVariableID idCNPara;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		idEPara = idEPressureGradient;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		idEPara = idERhoGradient;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	
	m_vara->cell(idCPara) = 0.;

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->edge(idEPara, i) = 0.;
	}

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->corner(idCNPara, i) = 0.;
	}
}

static void append_refresh_snapshot(
	p4est_t *p4est,
	GhostSession &session,
	std::vector<unsigned char> &snapshot)
{
	const size_t ghost_count =
		session.empty() ? 0 : session.get()->ghosts.elem_count;
	const size_t snapshot_size =
		(static_cast<size_t>(p4est->local_num_quadrants) + ghost_count) *
		sizeof(quad_data_t);
	snapshot.clear();
	snapshot.reserve(snapshot_size);

	for (p4est_topidx_t tree_id = p4est->first_local_tree;
		tree_id <= p4est->last_local_tree; ++tree_id) {
		p4est_tree_t *tree = p4est_tree_array_index(p4est->trees, tree_id);
		for (size_t index = 0; index < tree->quadrants.elem_count; ++index) {
			p4est_quadrant_t *quad =
				p4est_quadrant_array_index(&tree->quadrants, index);
			const unsigned char *bytes = static_cast<const unsigned char *>(
				quad->p.user_data);
			snapshot.insert(snapshot.end(), bytes, bytes + sizeof(quad_data_t));
		}
	}

	const unsigned char *ghost_bytes = NULL;
	size_t ghost_size = 0;
	if (!session.empty()) {
		ghost_bytes = reinterpret_cast<const unsigned char *>(session.data());
		ghost_size = ghost_count * sizeof(quad_data_t);
	}
	if (ghost_size > 0) {
		snapshot.insert(
			snapshot.end(), ghost_bytes, ghost_bytes + ghost_size);
	}
}

static void
refresh_after_balance(p4est_t *p4est, GhostSession &session)
{
	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		quadrant_update_after_balance_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_reset_hanging_info_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}


static void
postprocess_after_coarsening(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		NULL,
		quadrant_update_after_coarsening_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void
Gradient_estimate(p4est_t *p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	GhostCallbackContext callback_context = { &session };

	p4est_iterate(p4est,
		session.get(),
		(void *)session.data(),
		quadrant_set_gradient_zero_estimate_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);


	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		quadrant_edge_minmod_estimate_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		session.get(),
		(void *)session.data(),
		NULL,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		quadrant_corner_minmod_estimate_callback);


	p4est_iterate(p4est,
		NULL,
		(void *)p4est_data,
		quadrant_cell_minmod_estimate_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void PreProcess(p4est_t *p4est, GhostSession &session)
{

	Gradient_estimate(p4est, session);

	set_default_coarsening_tag(p4est);


	set_default_refining_tag(p4est);
}


static void write_balance_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_balance_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est,NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void write_coarsen_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_coarsen_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est, NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void write_refine_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_refine_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est, NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void quadrant_write_distance_profiles_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;

	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_lag, i); }

	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	double distance;

	if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::radiusType)
	{
		distance = sqrt(pow(center_point.x, 2) + pow(center_point.y,2));
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::xType)
	{
		distance = fabs(center_point.x);
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::yType)
	{
		distance = fabs(center_point.y);
	}
	p4est_data->DistanceFile << blank << blank << distance <<
		blank << blank << m_vara->cell(idDensity_lag) <<
		blank << blank << m_vara->cell(idPressure_lag) <<
		blank << blank << m_vara->cell(idInternalEnergy_lag) <<
		blank << blank << m_vara->cell(idTotalEnergy_lag) << endl;
}

static void write_distance_profiles(p4est_t *p4est)
{
	p4est_data_t		*p4est_data = (p4est_data_t*)p4est->user_pointer;
	int ret;
#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}
	p4est_iterate(p4est, NULL,
		(void *)p4est_data,
		quadrant_write_distance_profiles_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);
}

static void write_solution(p4est_t *p4est, const IOAlgorithm::OutputStamp &stamp)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	int					ret;

#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}

#ifdef _WIN32
	const char* path_format = "output\\" P4EST_STRING "_Lagrangian_%04d";
#else
	const char* path_format = "output/"P4EST_STRING "_Lagrangian_%04d";
#endif

	snprintf(filename, BUFSIZ, path_format, stamp.file_step);

	numquads = p4est->local_num_quadrants;

	sc_array_t		*coord_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*coord_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*pressure_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*temperature_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*rho_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*internal_energy_array = sc_array_new_size(sizeof(double), numquads);

	vtu_cell_data_t		m_cell_data;
	m_cell_data.density_array = rho_array;
	m_cell_data.pressure_array = pressure_array;
	m_cell_data.temperature_array = temperature_array;
	m_cell_data.internal_energy_array = internal_energy_array;
	m_cell_data.coordx = coord_x_array;
	m_cell_data.coordy = coord_y_array;
	m_cell_data.velox = velo_x_array;
	m_cell_data.veloy = velo_y_array;


	p4est_iterate(p4est, NULL,
		&m_cell_data,
		quadrant_copy_variable_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	
	context = p4est_vtk_write_cell_dataf(
		context, 
		0,
		1,
		1,
		0,
		3,
		0,
		"Pressure", 
		pressure_array,
		"density", 
		rho_array,
		"internal_energy",
		internal_energy_array,
		context
	);
	SC_CHECK_ABORT(context != NULL,
		P4EST_STRING "_vtk:Error:writing cell data");

	context = p4est_vtk_write_point_dataf(context, 4, 0,
		"NodeX", coord_x_array,
		"NodeY", coord_y_array,
		"NodeU", velo_x_array,
		"NodeV", velo_y_array,
		context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x_array);
	sc_array_destroy(coord_y_array);
	sc_array_destroy(velo_x_array);
	sc_array_destroy(velo_y_array);
	sc_array_destroy(pressure_array);
	sc_array_destroy(rho_array);
	sc_array_destroy(internal_energy_array);
	sc_array_destroy(temperature_array);

	
	if (p4est->mpirank == 0) {
		char pvtu_filename[1024];
		snprintf(pvtu_filename, sizeof(pvtu_filename), "%s.pvtu", filename);
		FILE *f = fopen(pvtu_filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *string = (char *)malloc(fsize + 1);
			fread(string, 1, fsize, f);
			fclose(f);
			string[fsize] = 0;
			
			char *insert_pos = strstr(string, "</VTKFile>");
			if (insert_pos) {
				*insert_pos = '\0';
				f = fopen(pvtu_filename, "wb");
				if (f) {
					fprintf(f, "%s", string);
					fprintf(f,
						"  <FieldData>\n"
						"    <DataArray type=\"Float64\" Name=\"TimeValue\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %.16g\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"FileStep\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"StateStep\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"OutputPhase\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"  </FieldData>\n</VTKFile>\n",
						stamp.time,
						stamp.file_step,
						stamp.state_step,
						IOAlgorithm::phase_code(stamp.phase));
					fclose(f);
				}
			}
			free(string);
		}
	}
}


struct InvariantContext
{
	int phase_id;
	int violations;
	p4est_topidx_t first_tree;
	int first_level;
	p4est_qcoord_t first_x;
	p4est_qcoord_t first_y;
	const char *first_name;
};

static void invariant_volume_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	InvariantContext *ctx = (InvariantContext *)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	const CVariable &vara = data->m_vara;
	std::vector<Diagnostics::InvariantViolation> v =
		Diagnostics::check_cell_invariants(vara);
	if (v.empty()) {
		return;
	}
	if (ctx->violations == 0) {
		ctx->first_tree = info->treeid;
		ctx->first_level = info->quad->level;
		ctx->first_x = info->quad->x;
		ctx->first_y = info->quad->y;
		ctx->first_name = v[0].name;
	}
	ctx->violations += (int)v.size();
	P4EST_GLOBAL_PRODUCTIONF(
		"  [phase %d] (tree=%lld, level=%d, x=%lld, y=%lld) %s: "
		"expected %.17e, got %.17e\n",
		ctx->phase_id, (long long)info->treeid, info->quad->level,
		(long long)info->quad->x, (long long)info->quad->y,
		v[0].name, v[0].expected, v[0].actual);
}

static void check_state_invariants(p4est_t *p4est, int phase_id)
{
	InvariantContext ctx = {phase_id, 0, -1, -1, -1, -1, NULL};
	p4est_iterate(p4est, NULL, &ctx, invariant_volume_callback, NULL,
#ifdef P4_TO_P8
		NULL,
#endif
		NULL);
	if (ctx.violations > 0) {
		P4EST_GLOBAL_PRODUCTIONF(
			"STATE INVARIANT VIOLATION (phase=%d): %d violation(s); "
			"first at (tree=%lld, level=%d, x=%lld, y=%lld) [%s]\n",
			ctx.phase_id, ctx.violations, (long long)ctx.first_tree,
			ctx.first_level, (long long)ctx.first_x, (long long)ctx.first_y,
			ctx.first_name ? ctx.first_name : "");
		std::abort();
	}
}

static void advance_time_step(p4est_t * p4est, double start_time, double end_time)
{
	double              t = start_time;
	double              dt = 0.;
	GhostSession ghost_session;
	p4est_data_t		*p4est_data = (p4est_data_t *)p4est->user_pointer;
	int					recursive = 0;
	int					allowed_level = p4est_data->max_level;
	int					callbackorphans = 0;
	int					allowcoarsening = 1;

	
	ghost_session.initialize(p4est, P4EST_CONNECT_FULL);

	for (t = start_time; t < end_time; t += p4est_data->delta_time)
	{
		p4est_data->current_step += 1;
		trace_target_snapshot(p4est, "STEP_BEGIN");
		//StatGlobalFieldChecksum(p4est, "Checkpoint 1: Start time loop");
		if(p4est_data->current_step>p4est_data->max_time_step)
		{
			P4EST_GLOBAL_PRODUCTIONF("The current step %d is larger than the max step %d, simulation is stopped!\n",
				p4est_data->current_step, p4est_data->max_time_step);
			break;
		}
		int current_output_index = (int)(p4est_data->current_time / p4est_data->write_interval_time);

		
		PreProcess(p4est, ghost_session);
		trace_target_snapshot(p4est, "AFTER_PREPROCESS");

		
		if (p4est_data->current_step && !(p4est_data->current_step%p4est_data->refine_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)

		{
			AMR_DEBUG_LOG("DEBUG: Entering p4est_refine_ext\n");
			p4est_refine_ext(p4est, recursive, allowed_level,
				Lagrangian_refine_err_estimate, NULL,
				Lagrangian_replace_quads);
			AMR_DEBUG_LOG("DEBUG: Finished p4est_refine_ext\n");

			ghost_session.invalidate_after_topology_change();
			AMR_DEBUG_LOG("DEBUG: Entering GhostSession rebuild\n");
			ghost_session.rebuild(p4est, P4EST_CONNECT_FULL);
			AMR_DEBUG_LOG("DEBUG: Finished GhostSession rebuild\n");

			AMR_DEBUG_LOG("DEBUG: Entering set_allowing_coarsening_tag\n");
			set_allowing_coarsening_tag(p4est, ghost_session);
			AMR_DEBUG_LOG("DEBUG: Finished set_allowing_coarsening_tag\n");

			AMR_DEBUG_LOG("DEBUG: Entering p4est_coarsen_ext\n");
			p4est_coarsen_ext(p4est, recursive, callbackorphans,
			Lagrangian_coarsen_err_estimate, NULL,
			Lagrangian_replace_quads);
			AMR_DEBUG_LOG("DEBUG: Finished p4est_coarsen_ext\n");

			StatTotalEnergyError(p4est);
			p4est_balance_ext(p4est, P4EST_CONNECT_CORNER, NULL,
				Lagrangian_replace_quads);

			ghost_session.invalidate_after_topology_change();
			ghost_session.destroy();
		}
		//StatGlobalFieldChecksum(p4est, "Checkpoint 2: AMR");


		if (p4est_data->current_step &&
			!(p4est_data->current_step%p4est_data->repartition_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)
		{
			p4est_partition(p4est, allowcoarsening, NULL);
			ghost_session.invalidate_after_topology_change();
			ghost_session.destroy();
		}


		if (ghost_session.empty())
		{
			ghost_session.initialize(p4est, P4EST_CONNECT_FULL);
		}


		refresh_after_balance(p4est, ghost_session);
		if (refresh_idempotence_check_enabled()) {
			std::vector<unsigned char> first_refresh;
			std::vector<unsigned char> second_refresh;
			append_refresh_snapshot(p4est, ghost_session, first_refresh);
			refresh_after_balance(p4est, ghost_session);
			append_refresh_snapshot(p4est, ghost_session, second_refresh);
			const int local_match = first_refresh == second_refresh ? 1 : 0;
			int global_match = 0;
			sc_MPI_Allreduce(&local_match, &global_match, 1, sc_MPI_INT,
				sc_MPI_MIN, p4est->mpicomm);
			SC_CHECK_ABORT(global_match,
				"refresh_after_balance is not idempotent");
		}
		trace_target_snapshot(p4est, "AFTER_AMR_REFRESH");

		ghost_session.exchange();

		
		if (p4est_data->equal_dt == false) { predict_timestep(p4est); }

		
		const IOAlgorithm::OutputStamp output_stamp =
			IOAlgorithm::make_pre_step_stamp(
				p4est_data->current_step, p4est_data->current_time);
		if (!(p4est_data->current_step % p4est_data->write_interval_step))
		{
			write_solution(p4est, output_stamp);
		}
		else if (current_output_index > p4est_data->last_output_index)
		{
			p4est_data->last_output_index = current_output_index;
			write_solution(p4est, output_stamp);
		}
		else if (p4est_data->current_time+ p4est_data->delta_time >= p4est_data->end_time)
		{
			write_solution(p4est, output_stamp);
		}

		
		advance_single_stage(p4est, ghost_session);

		
		StatTotalEnergyError(p4est);

		
		AcceptNumericalSolution(p4est);

		if (state_invariant_check_enabled()) {
			check_state_invariants(p4est, 1);
		}

		p4est_data->current_time = p4est_data->current_time + p4est_data->delta_time;

		
		P4EST_GLOBAL_PRODUCTIONF("simulation_step= %d, delta_time = %.10lf, simulation_time = %.6lf \n",
			p4est_data->current_step, p4est_data->delta_time, p4est_data->current_time);
	}
	write_distance_profiles(p4est);
	ghost_session.destroy();
}

namespace IOAlgorithm {

static void debug_quadrant_copy_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	debug_vtu_cell_data_t *m_cell_data = (debug_vtu_cell_data_t *)user_data;
	p4est_t *p4est = info->p4est;
	p4est_tree_t *tree = p4est_tree_array_index(p4est->trees, info->treeid);
	quad_data_t *quad_data = (quad_data_t *)info->quad->p.user_data;
	
	p4est_locidx_t local_id = info->quadid + tree->quadrants_offset;
	
	// Global SFC ID
	double global_id = (double)(p4est->global_first_quadrant[p4est->mpirank] + local_id);
	*(double *)sc_array_index(m_cell_data->global_sfc_id_array, local_id) = global_id;

	CVariable *m_vara = (CVariable *)&quad_data->m_vara;

	*(double *)sc_array_index(m_cell_data->density_array, local_id) =
		m_vara->cell(idDensity_lag);
	*(double *)sc_array_index(m_cell_data->pressure_array, local_id) =
		m_vara->cell(idPressure_lag);
	*(double *)sc_array_index(m_cell_data->internal_energy_array, local_id) =
		m_vara->cell(idInternalEnergy_lag);

	// Corner pressures (using hdata[0].pi as proxy for half-edge pressure)
	*(double *)sc_array_index(m_cell_data->pressure_c0_array, local_id) = quad_data->m_cndata[0].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c1_array, local_id) = quad_data->m_cndata[1].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c2_array, local_id) = quad_data->m_cndata[2].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c3_array, local_id) = quad_data->m_cndata[3].hdata[0].pi;

	// Corner velocities
	*(double *)sc_array_index(m_cell_data->velou_c0_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 0).x;
	*(double *)sc_array_index(m_cell_data->velou_c1_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 1).x;
	*(double *)sc_array_index(m_cell_data->velou_c2_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 2).x;
	*(double *)sc_array_index(m_cell_data->velou_c3_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 3).x;

	*(double *)sc_array_index(m_cell_data->velov_c0_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 0).y;
	*(double *)sc_array_index(m_cell_data->velov_c1_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 1).y;
	*(double *)sc_array_index(m_cell_data->velov_c2_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 2).y;
	*(double *)sc_array_index(m_cell_data->velov_c3_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 3).y;
}

void p4est_debug_output_vtu(p4est_t *p4est, const char *prefix, int step, int location_id)
{
	char filename[1024];
	snprintf(filename, sizeof(filename), "%s_checkpoint_%04d_loc%d", prefix, step, location_id);

	p4est_locidx_t numquads = p4est->local_num_quadrants;

	debug_vtu_cell_data_t m_cell_data;
	m_cell_data.global_sfc_id_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.density_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.internal_energy_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c3_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c3_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c3_array = sc_array_new_size(sizeof(double), numquads);

	p4est_iterate(p4est, NULL, &m_cell_data, debug_quadrant_copy_variable_to_array_callback, NULL,
#ifdef P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_vtk_context_t *context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);

	context = p4est_vtk_write_cell_dataf(
		context, 1, 1, 1, 0,
		16, 0,
		"Global_SFC_ID", m_cell_data.global_sfc_id_array,
		"Density", m_cell_data.density_array,
		"Pressure", m_cell_data.pressure_array,
		"InternalEnergy", m_cell_data.internal_energy_array,
		"Pressure_c0", m_cell_data.pressure_c0_array,
		"Pressure_c1", m_cell_data.pressure_c1_array,
		"Pressure_c2", m_cell_data.pressure_c2_array,
		"Pressure_c3", m_cell_data.pressure_c3_array,
		"VelocityU_c0", m_cell_data.velou_c0_array,
		"VelocityU_c1", m_cell_data.velou_c1_array,
		"VelocityU_c2", m_cell_data.velou_c2_array,
		"VelocityU_c3", m_cell_data.velou_c3_array,
		"VelocityV_c0", m_cell_data.velov_c0_array,
		"VelocityV_c1", m_cell_data.velov_c1_array,
		"VelocityV_c2", m_cell_data.velov_c2_array,
		"VelocityV_c3", m_cell_data.velov_c3_array,
		context
	);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing cell data");

	int retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");

	sc_array_destroy(m_cell_data.global_sfc_id_array);
	sc_array_destroy(m_cell_data.pressure_array);
	sc_array_destroy(m_cell_data.density_array);
	sc_array_destroy(m_cell_data.internal_energy_array);
	sc_array_destroy(m_cell_data.pressure_c0_array);
	sc_array_destroy(m_cell_data.pressure_c1_array);
	sc_array_destroy(m_cell_data.pressure_c2_array);
	sc_array_destroy(m_cell_data.pressure_c3_array);
	sc_array_destroy(m_cell_data.velou_c0_array);
	sc_array_destroy(m_cell_data.velou_c1_array);
	sc_array_destroy(m_cell_data.velou_c2_array);
	sc_array_destroy(m_cell_data.velou_c3_array);
	sc_array_destroy(m_cell_data.velov_c0_array);
	sc_array_destroy(m_cell_data.velov_c1_array);
	sc_array_destroy(m_cell_data.velov_c2_array);
	sc_array_destroy(m_cell_data.velov_c3_array);
}

} // namespace IOAlgorithm

int main(int argc, char **argv)
{
	int                 mpiret;
	sc_MPI_Comm         mpicomm;
	p4est_data_t		ctx;

	mpiret = sc_MPI_Init(&argc, &argv);
	SC_CHECK_MPI(mpiret);
	mpicomm = sc_MPI_COMM_WORLD;

	sc_init(mpicomm, 1, 1, NULL, SC_LP_ESSENTIAL);
	p4est_init(NULL, SC_LP_PRODUCTION);

	IOAlgorithm::ConfigParser cfg("param.ini");
	ctx.load_from_config(cfg);
	SC_CHECK_ABORT(ctx.has_valid_simulation_settings(),
		"Invalid simulation configuration or clock settings");

	P4EST_GLOBAL_PRODUCTIONF("This is the p4est %dD demo for Lagrangian hydrodynamics\n", P4EST_DIM);

	
	p4est_connectivity_t *conn = p4est_connectivity_new_unitsquare();
	

	const SimulationModel::SimulationConfig startup_config =
		ctx.simulation_config();
	p4est_t *p4est = p4est_new_ext(mpicomm,				 
		conn,					 
		1,						 
		startup_config.mesh.minimum_level,						 
		1,						 
		sizeof(quad_data_t), 
		Lagrangian_init_condition,
		(void *)(&ctx));          

	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	
	int recursive = 1;
	

	int partforcoarsen = 1;

	
	p4est_balance(p4est, P4EST_CONNECT_CORNER, Lagrangian_init_condition);
	p4est_partition(p4est, partforcoarsen, NULL);

	if (state_invariant_check_enabled()) {
		P4EST_GLOBAL_PRODUCTIONF(
			"[invariant-checker] enabled; checking post-init and post-accept states\n");
		check_state_invariants(p4est, 0);
	}

	// Test call to the debug VTU output
	//IOAlgorithm::p4est_debug_output_vtu(p4est, "output/debug_checkpoint", 0, 0);

	const SimulationModel::SimulationClock startup_clock =
		p4est_data->simulation_clock();
	advance_time_step(p4est,
		startup_clock.start_time,
		startup_clock.end_time);

								   
	p4est_destroy(p4est);
	p4est_connectivity_destroy(conn);

	sc_finalize();
	mpiret = sc_MPI_Finalize();
	SC_CHECK_MPI(mpiret);
	return 0;
}