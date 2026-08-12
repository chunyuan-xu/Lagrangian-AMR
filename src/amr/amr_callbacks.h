#pragma once
#include <algorithm>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "mesh/ghost_context.h"
#include "variable.h"
#include "physics/physics_alg.h"
#include "physics/timestep_reduction.h"
#include "amr/amr_transfer.h"
#include "core/trace.h"

// M8.1: AMRCallbacks — AMR-domain quadrant callbacks stripped from main.cpp.
// Each is a pure per-quadrant function over the p4est iterate context.

// Forward declaration avoids a hydro_callbacks.h <-> amr_callbacks.h
// include cycle: hydro_callbacks.h references AMRCallbacks:: and
// amr_callbacks.h (M9.1.3) references HydroCallbacks::. main.cpp includes
// amr_callbacks.h before hydro_callbacks.h, so the full definition is
// visible at the call site.
namespace HydroCallbacks {
void generate_children_info_from_parent(p4est_data_t *p4est_data, CVariable *m_vara);
}

namespace AMRCallbacks {

void quadrant_reset_parent_edge_callback(p4est_iter_volume_info_t *info, void *user_data);
void quadrant_get_children_hanging_info_callback(p4est_iter_face_info_t *info, void *user_data);
void quadrant_reset_hanging_info_callback(p4est_iter_volume_info_t *info, void *user_data);


void quadrant_predict_timestep_callback(p4est_iter_volume_info_t *info, void *user_data)
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


void get_hanging_edge_info_from_logical_position(const int which_face, const p4est_qcoord_t qx1, const p4est_qcoord_t qy1,
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
void quadrant_edge_minmod_estimate_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
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
				m_child1_read_vara = &context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_cndata = (CCorner_data *)&context->session->remote(side[i]->is.hanging.quadid[0]).m_cndata;
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
				m_child2_read_vara = &context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_cndata = (CCorner_data *)&context->session->remote(side[i]->is.hanging.quadid[1]).m_cndata;
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
				m_parent_read_vara = &context->session->remote(side[full_index]->is.full.quadid).m_vara;
				m_parent_cndata = (CCorner_data *)&context->session->remote(side[full_index]->is.full.quadid).m_cndata;
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
void quadrant_cell_minmod_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
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

void quadrant_whether_allowing_coarsening_from_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
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
void quadrant_update_after_balance_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
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
			AMRCallbacks::get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&context->session->remote(side[i]->is.hanging.quadid[0]);
				m_child1_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[0]).m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&context->session->remote(side[i]->is.hanging.quadid[1]);
				m_child2_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[1]).m_cndata);
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
				m_parent_data = (quad_data_t *)&context->session->remote(side[full_index]->is.full.quadid);
				m_parent_vara = (CVariable *)&context->session->remote(side[full_index]->is.full.quadid).m_vara;
				m_parent_cndata = (CCorner_data *)&context->session->remote(side[full_index]->is.full.quadid).m_cndata;
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
void quadrant_set_init_parent_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
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
			AMRCallbacks::get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = context->session->data() + side[i]->is.hanging.quadid[0];
				m_child1_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[0]).m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = context->session->data() + side[i]->is.hanging.quadid[1];
				m_child2_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[1]).m_cndata);
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
				m_parent_data = context->session->data() + side[full_index]->is.full.quadid;
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

void
quadrant_whether_allowing_coarsening_from_corner_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	sc_array_t	*sides = &(info->sides);
	int	is_ghost_a, m_size;
	int			quadid_a, quadid_b;
	CVariable		*m_vara_a;

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
			m_vara_a = (CVariable  *)&context->session->remote(quadid_a).m_vara;
		}
		else
		{
			m_vara_a = (CVariable  *)&((quad_data_t *)side_i->quad->p.user_data)->m_vara;
		}
		
		for (int j = 0; j < m_size; j++)
		{
			if (j == i) { continue; }
			p4est_iter_corner_side_t *side_j = p4est_iter_cside_array_index_int(sides, j);
			quadid_b = side_j->quadid;
			p4est_quadrant	*quad_b = side_j->quad;
			int level_b = quad_b->level;
			
			if (level_b - level_a > 1)
			{
				if (!is_ghost_a)
				{
					m_vara_a->int_cell(idAllowCoarsening) = p4est_data_t::CoarseningEnum::CoarsingNotAllowed;
				}
			}
		}
	}
}
void
quadrant_set_default_coarsening_tag_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	
	
	data->m_vara.int_cell(idCoarseningTag) = p4est_data_t::CoarseningEnum::NotCoarsenedJustNow;

	
	data->m_vara.int_cell(idAllowCoarsening) = p4est_data_t::CoarseningEnum::CoarsingAllowed;
}
void
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

void set_default_coarsening_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		AMRCallbacks::quadrant_set_default_coarsening_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}
void 
set_default_refining_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		AMRCallbacks::quadrant_set_default_refining_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}
void
set_allowing_coarsening_tag(p4est_t *p4est, GhostSession &session)
{
	GhostCallbackContext callback_context = { &session };

	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		AMRCallbacks::quadrant_whether_allowing_coarsening_from_edge_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		AMRCallbacks::quadrant_whether_allowing_coarsening_from_corner_callback);
}

void Get_AMR_BDY_info(p4est_t *p4est, GhostSession &session)
{
	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		quadrant_get_children_hanging_info_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

	if (!session.empty()) {
		session.exchange();
	}


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
		&callback_context,
		NULL,
		AMRCallbacks::quadrant_set_init_parent_edge_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}
void append_refresh_snapshot(
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
void
refresh_after_balance(p4est_t *p4est, GhostSession &session)
{
	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		AMRCallbacks::quadrant_update_after_balance_callback,
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

void
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
void
quadrant_get_children_hanging_info_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
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

			AMRCallbacks::get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);

			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&context->session->remote(side[i]->is.hanging.quadid[0]);
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}

			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&context->session->remote(side[i]->is.hanging.quadid[1]);
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
void
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

// M9.1.3: p4est refine/coarsen replace callback (parent-child transfer).
void
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


		AMRTransfer::coarsen_children_to_parent(p4est_data, parent_data,
			child_data1, child_data2, child_data3, child_data4);
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


			AMRTransfer::refine_distribute_buffers(parent_data, child_data, i, children_coord);

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
			HydroCallbacks::generate_children_info_from_parent(p4est_data, child_vara);
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
} // namespace AMRCallbacks
