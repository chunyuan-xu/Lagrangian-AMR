#pragma once
#include <p4est.h>
#include "defines.h"
#include "core/trace.h"
#include "variable.h"
#include "mesh/ghost_context.h"
#include "physics/corner_solve.h"

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
			if (is_boundary)
			{


				m_data->points[cnid].velo_lag = CornerSolve::boundary_node_velocity(
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
void quadrant_parent_edge_matrix_callback(p4est_iter_volume_info_t *info, void *user_data)
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
void quadrant_accept_center_solution_callback(p4est_iter_volume_info_t *info, void *user_data)
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
void quadrant_compute_corner_force_callback(p4est_iter_volume_info_t *info, void *user_data)
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
} // namespace HydroCallbacks
