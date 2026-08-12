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

} // namespace HydroCallbacks
