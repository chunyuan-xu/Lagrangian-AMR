#pragma once
#include <p4est.h>
#include "defines.h"
#include "core/trace.h"
#include "variable.h"
#include "mesh/ghost_context.h"
#include "amr/refinement_variable_selector.h"
#include "amr/gradient_kernels.h"
#include "physics/corner_solve.h"
#include "hydro/parent_edge_force.h"
#include "hydro/corner_matrix_kernel.h"
#include "hydro/corner_velocity_kernel.h"
#include "hydro/parent_edge_matrix_kernel.h"
#include "diagnostics/hydro_trace.h"
#include "nodal/boundary_mirror_runtime.h"
#include "nodal/epoch_runtime.h"
#include "nodal/face_geometry_mirror_runtime.h"
#include "nodal/local_master_runtime.h"

// M8.2: HydroCallbacks — hydro-domain quadrant callbacks stripped from main.cpp.

namespace HydroCallbacks {


int convert_which_corner_to_user_define_index(const int &which_corner)
{
	int m_index;
	if (which_corner == p4est_enum_corner::left_bottom) { m_index = quad_data_t::EnumCorner::LEFTBOTTOM; }
	if (which_corner == p4est_enum_corner::right_bottom) { m_index = quad_data_t::EnumCorner::RIGHTBOTTOM; }
	if (which_corner == p4est_enum_corner::left_up) { m_index = quad_data_t::EnumCorner::LEFTUP; }
	if (which_corner == p4est_enum_corner::right_up) { m_index = quad_data_t::EnumCorner::RIGHTUP; }
	return m_index;
}

void quadrant_corner_matrix_assemble_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&(data->m_cndata);
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int				CoordType = p4est_data->coord_type;

	for (int k = 0; k < CNDIM; k++)
	{
		build_corner_matrix_rhs(*m_vara, cndata[k], CoordType,
			p4est_data->solver_type, k);
		if (target_trace_enabled() && p4est_data->current_step == 3 && is_trace_parent(info->quad) && (k == 0 || k == 3)) {
			FILE *f = open_corner2_trace(info->p4est);
			if (f) {
				fprintf(f, "TRACE stage=LOCAL_CORNER iter=%d corner=%d", trace_riemann_iter(), k);
				trace_matrix(f, "idcnMcp", m_vara->MarCnData[idcnMcp][k]);
				trace_vector(f, "idcnRHS", m_vara->corner_vector(idcnRHS, k));
				trace_vector(f, "velocity_in", m_vara->corner_vector(idcnVelocity_lag, k));
				fprintf(f, "\n");
				fclose(f);
			}
		}
	}
}

void quadrant_copy_velocity_from_lag_to_relax_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	
	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->corner_vector(idcnVelocity_relaxed, k) = m_vara->corner_vector(idcnVelocity_lag, k);
	}
}

void quadrant_corner_velocity_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_iter_corner_side_t	*side[CNDIM];  
	sc_array_t					*sides = &(info->sides);
	int							which_corner, cnid, is_ghost, m_size;
	int							quadid;
	int							tree_boundary;
	bool						is_boundary ;
	quad_data_t					*m_data;
	CVariable					*m_vara;
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);

	
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
			m_data = (quad_data_t *)&context->session->remote(quadid);
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
			m_data = (quad_data_t *)&context->session->remote(quadid);
			m_vara = (CVariable *)&context->session->remote(quadid).m_vara;
		}
		else
		{
			m_data = (quad_data_t *)side[i]->quad->p.user_data;
			m_vara = (CVariable *)&m_data->m_vara;
		}


		if (!is_ghost)
		{
			m_data->points[cnid].velo_lag = solve_regular_corner_velocity(
				is_boundary,
				m_data->points[cnid].TwoBouns[0],
				m_data->points[cnid].TwoBouns[1],
				m_data->points[cnid].MatrixP,
				m_data->points[cnid].RHS);
			m_vara->corner_vector(idcnVelocity_lag, cnid) = m_data->points[cnid].velo_lag;
		}
		p4est_data_t *p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
		if (target_trace_enabled() && p4est_data->current_step == 3 && !is_ghost &&
			((is_trace_fine(side[i]->quad) && cnid == 2) ||
			 (is_trace_parent(side[i]->quad) && (cnid == 0 || cnid == 3)))) {
			FILE *f = open_corner2_trace(info->p4est);
			if (f) {
				fprintf(f, "TRACE stage=CORNER_SOLVE iter=%d cell=(%d,%d,L%d,c%d) hanging=%d", trace_riemann_iter(),
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


void generate_children_info_from_parent(p4est_data_t *p4est_data, CVariable *m_vara)
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
void quadrant_compute_halftime_variable_callback(p4est_iter_volume_info_t *info, void *user_data)
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
		P4EST_GLOBAL_PRODUCTIONF("The half time internal energy is illegal\n");
		abort();
	}
	m_vara->cell(idPressure_half) = PhysicalAlg::EquationOfState(m_vara->cell(idGamma), m_vara->cell(idDensity_half), m_vara->cell(idInternalEnergy_half));
	if (m_vara->cell(idPressure_half) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The half time pressure is illegal\n");
		abort();
	}
}
void quadrant_parent_edge_matrix_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t		*)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable		*)&data->m_vara;
	ParentBounInfo	*PCInfo = (ParentBounInfo	*)&data->m_pc_edge_data;
	p4est_data_t *p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;

	for (int k = 0; k < CNDIM; k++)
	{
		
		if (PCInfo[k].IsParentChildBoun == true)
		{
			Diagnostics::trace_parent_edge_matrix(info, k);
			build_parent_edge_matrix_rhs(*m_vara, PCInfo[k], k);
			if (target_trace_enabled() && p4est_data->current_step == 3 && is_trace_parent(info->quad)) {
				FILE *f = open_corner2_trace(info->p4est);
				if (f) {
					fprintf(f, "TRACE stage=PARENT_EDGE iter=%d face=%d is_pc=%d", trace_riemann_iter(), k, PCInfo[k].IsParentChildBoun ? 1 : 0);
					trace_vector(f, "hanging_in", PCInfo[k].Hanging_velocity);
					fprintf(f, " L=(%.17e,%.17e)", PCInfo[k].Lcp[0], PCInfo[k].Lcp[1]);
					trace_vector(f, "N0", PCInfo[k].Ncp[0]);
					trace_vector(f, "N1", PCInfo[k].Ncp[1]);
					fprintf(f, " Z=%.17e", PCInfo[k].Zcp);
					trace_matrix(f, "ideMcp", m_vara->MarCnData[ideMcp][k]);
					trace_vector(f, "ideRHS", m_vara->corner_vector(ideRHS, k));
					fprintf(f, "\n");
					fclose(f);
				}
			}
		}
	}
}
void quadrant_accept_center_solution_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
void quadrant_compute_corner_force_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
		m_vara->corner_vector(ideFcp, eind) = ParentEdgeForce::evaluate(
			PCInfo[eind],
			m_vara->corner_vector(idReconstructVelocity, eind),
			m_vara->corner(idReconstructPressure, eind),
			m_vara->MarCnData[ideMcp][eind]);
	}
}
void quadrant_flux_relaxed_reset_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	CCorner_data		*cndata = (CCorner_data *)&data->m_cndata;


	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->corner_vector(idcnFluxRelaxed, k) = CDoubleVector(0.,0.);
	}
}

void quadrant_compute_RcpLcpNcp_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
void quadrant_compute_relaxed_info_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
void quadrant_relaxed_hanging_solver_callback(p4est_iter_face_info_t *info, void *user_data)
{
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
			AMRCallbacks::get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = context->session->data() + side[i]->is.hanging.quadid[0];
				m_child1_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_read_vara = &context->session->remote(side[i]->is.hanging.quadid[0]).m_vara;
				m_child1_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[0]).m_cndata);
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
				m_child2_data = context->session->data() + side[i]->is.hanging.quadid[1];
				m_child2_vara = (CVariable *)&context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_read_vara = &context->session->remote(side[i]->is.hanging.quadid[1]).m_vara;
				m_child2_cndata = (CCorner_data *)&(context->session->remote(side[i]->is.hanging.quadid[1]).m_cndata);
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
				m_parent_data = context->session->data() + side[full_index]->is.full.quadid;
				m_parent_vara = (CVariable *)&context->session->remote(side[full_index]->is.full.quadid).m_vara;
				m_parent_read_vara = &context->session->remote(side[full_index]->is.full.quadid).m_vara;
				m_parent_cndata = (CCorner_data *)&context->session->remote(side[full_index]->is.full.quadid).m_cndata;
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
						trace_riemann_iter(), quad_child1->x, quad_child1->y, quad_child1->level, m_which_corner[0], side[i]->is.hanging.is_ghost[0] ? 1 : 0,
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
void quadrant_get_BYD_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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
void quadrant_mirror_boundary_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	(void)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	Nodal::BoundaryMirrorError err = Nodal::mirror_legacy_boundary_to_faces(*data);
	if (err.failed) {
		P4EST_GLOBAL_PRODUCTIONF("ERROR: L2 boundary mirror failed for local leaf: %s\n",
			err.reason ? err.reason : "unknown");
	}
}
void quadrant_mirror_face_geometry_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	(void)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	Nodal::FaceGeometryMirrorError err = Nodal::mirror_legacy_geometry_to_faces(*data);
	if (err.failed) {
		P4EST_GLOBAL_PRODUCTIONF("ERROR: L3 face geometry mirror failed for local leaf: %s\n",
			err.reason ? err.reason : "unknown");
	}
}
void quadrant_stage_reset_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	Nodal::StageResetContext *context = static_cast<Nodal::StageResetContext *>(user_data);
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	if (context == NULL) {
		P4EST_GLOBAL_PRODUCTIONF("ERROR: L5a stage reset context missing\n");
		return;
	}
	Nodal::stamp_stage_reset(*data, context->ctx);
}
void quadrant_invalidate_stage_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	(void)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	Nodal::invalidate_stage_reset(*data);
}
void quadrant_write_local_master_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	(void)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	Nodal::write_cell_local_master(*data);
}
void quadrant_validate_stage_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	Nodal::StageResetContext *context = static_cast<Nodal::StageResetContext *>(user_data);
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	if (context == NULL) {
		P4EST_GLOBAL_PRODUCTIONF("ERROR: S2c stage validation context missing\n");
		return;
	}
	Nodal::EpochError err = Nodal::validate_stage_reset(*data, context->ctx);
	if (err.failed) {
		P4EST_GLOBAL_PRODUCTIONF("ERROR: S2c local stamp invalid after publication: %s\n",
			err.reason ? err.reason : "unknown");
	}
}
void quadrant_update_corner_coordinate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
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

void
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
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);

	m_size = int(sides->elem_count);

	for (int i = 0; i < m_size; i++)
	{
		
		side[i] = p4est_iter_cside_array_index_int(sides, i);


		quadid = side[i]->quadid;

		which_corner = side[i]->corner;
		cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);

		
		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&context->session->remote(quadid);
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
		cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&context->session->remote(quadid);
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
		cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&context->session->remote(quadid);
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
			cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);

			is_ghost = side[i]->is_ghost;
			if (is_ghost)
			{
				m_data = (quad_data_t  *)&context->session->remote(quadid);
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
				cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);

				is_ghost = side[i]->is_ghost;
				if (is_ghost)
				{
					m_data = (quad_data_t  *)&context->session->remote(quadid);
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
void 
quadrant_hanging_point_matrix_assemble_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
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

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_full = (p4est_quadrant *)side[full_index]->is.full.quad;
			parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_quad_data_full = (quad_data_t *)&context->session->remote(side[full_index]->is.full.quadid);
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
						trace_riemann_iter(), quad->x, quad->y, quad->level, m_which_corner[0], side[i]->is.hanging.is_ghost[0] ? 1 : 0,
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
// M9.2.4: MUSCL corner-gradient limiter (zero estimate + corner minmod).
void
quadrant_set_gradient_zero_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_t			*p4est = info->p4est;

	const AMRCallbacks::RefinementVariableIds ids =
		AMRCallbacks::refinement_variable_ids(p4est_data->refine_coarsen_enum);
	const DoubleCellVariableID idCPara = ids.gradient_cell;
	const DoubleEdgeVariableID idEPara = ids.edge;
	const DoubleCornerVariableID idCNPara = ids.corner;


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

void quadrant_corner_minmod_estimate_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	which_corner, cnid, is_ghost, is_ghost_aside, m_size;
	int			quadid, quadid_aside;
	quad_data_t		*m_data, *m_data_aside;
	CVariable		*m_vara, *m_vara_aside;
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
	double			ParaGradient;

	const AMRCallbacks::RefinementVariableIds ids =
		AMRCallbacks::refinement_variable_ids(p4est_data->refine_coarsen_enum);
	const DoubleCellVariableID idCPara = ids.source_cell;
	const DoubleCornerVariableID idCNPara = ids.corner;

	m_size = int(sides->elem_count);


	for (int i = 0; i < m_size; i++)
	{

		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = HydroCallbacks::convert_which_corner_to_user_define_index(which_corner);


		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&context->session->remote(quadid);
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
				m_data_aside = (quad_data_t  *)&context->session->remote(quadid_aside);
			}
			else
			{
				m_data_aside = (quad_data_t  *)side[j]->quad->p.user_data;
			}
			m_vara_aside = (CVariable  *)&m_data_aside->m_vara;

			double m_dist = GeometryAlg::guarded_point_distance(
				m_vara->cell_vector(idCentroidCoord_cur), m_vara_aside->cell_vector(idCentroidCoord_cur),
				"AMR corner gradient");
			ParaGradient = abs(m_vara->cell(idCPara) - m_vara_aside->cell(idCPara)) / m_dist;
			if (!is_ghost) {
				m_vara->corner(idCNPara, cnid) = AMRCallbacks::reduce_max_corner_neighbor(
					m_vara->corner(idCNPara, cnid), ParaGradient);
			}
		}
	}
}
} // namespace HydroCallbacks
