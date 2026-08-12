#pragma once
#include <algorithm>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "mesh/ghost_context.h"
#include "variable.h"
#include "physics/physics_alg.h"
#include "physics/timestep_reduction.h"

// M8.1: AMRCallbacks — AMR-domain quadrant callbacks stripped from main.cpp.
// Each is a pure per-quadrant function over the p4est iterate context.

namespace AMRCallbacks {

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
} // namespace AMRCallbacks
