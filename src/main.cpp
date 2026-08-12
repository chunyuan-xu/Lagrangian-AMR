#include "alg.h"
#include "amr/amr_criteria.h"
#include "amr/amr_transfer.h"
#include "amr/amr_controller.h"
#include "amr/amr_callbacks.h"
#include "physics/corner_solve.h"
#include "solver/corner_solver.h"
#include "solver/solver_gate.h"
#include "solver/riemann_phases.h"
#include "solver/hydro_phases.h"
#include "hydro/hydro_callbacks.h"
#include "solver/hydro_callbacks.h"
#include "io/vtk_writer.h"
#include "io/io_callbacks.h"
#include "io/config_parser.h"
#include "io/output_stamp.h"
#include "physics/timestep_reduction.h"
#include "physics/stage_policy.h"
#include "diagnostics/state_invariant_checker.h"
#include "simulation/simulation.h"
#include "mesh/ghost_session.h"
#include "mesh/ghost_context.h"
#include "mesh/cell_key.h"
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



















void CalculateCornerRcpLcpNcp(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		HydroCallbacks::quadrant_compute_RcpLcpNcp_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
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
		HydroCallbacks::quadrant_get_BYD_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
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

	HydroCallbacks::generate_children_info_from_parent(p4est_data, m_vara);
}





static void 
predict_timestep(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_data->local_dt = TimestepReduction::initial_local_minimum();

	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		AMRCallbacks::quadrant_predict_timestep_callback,
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





void ComputeDivergence(p4est_t *p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_divergence_callback);
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
	GhostCallbackContext *context =
		static_cast<GhostCallbackContext *>(user_data);
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













void ComputeSoundSpeed(p4est_t *p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_soundspeed_callback);
}




void CalculateHalfTimeVariable(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;


	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		HydroCallbacks::quadrant_compute_halftime_variable_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
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





static void 
quadrant_hanging_point_matrix_assemble_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
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
		HydroCallbacks::quadrant_corner_matrix_assemble_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

	if (!session.empty()) {
		session.exchange();
	}

	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		quadrant_corner_to_point_matrix_assemble_callback);
}



void ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est_t *p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	GhostCallbackContext callback_context = { &session };


	p4est_iterate(p4est,
		NULL,
		NULL,
		HydroCallbacks::quadrant_compute_relaxed_info_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);


	p4est_iterate(p4est,
		NULL,
		NULL,
		HydroCallbacks::quadrant_parent_edge_matrix_callback,
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
		&callback_context,
		NULL,
		quadrant_hanging_point_matrix_assemble_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

		if (!session.empty()) {
			session.exchange();
		}

	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		HydroCallbacks::quadrant_relaxed_hanging_solver_callback,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}





static void ComputeCornerNodeVelocity(p4est_t * p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	GhostCallbackContext callback_context = { &session };
	p4est_iterate(p4est,
		session.get(),
		&callback_context,
		NULL,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		HydroCallbacks::quadrant_corner_velocity_callback);

	p4est_iterate(p4est,
		NULL,
		NULL,
		HydroCallbacks::quadrant_copy_velocity_from_lag_to_relax_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);
}





static void ComputeCoordinate(p4est_t * p4est)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		HydroCallbacks::quadrant_update_corner_coordinate_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}





static void UpdateDensity(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_density_callback);
}





static void UpdateMomentumEquation(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_momentum_callback);
}





static void ComputeWork(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_work_callback);
}





static void UpdateEnergyEquation(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_energy_callback);
}





static void UpdateEquationOfState(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_EOS_callback);
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



static void
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

static void Get_AMR_BDY_info(p4est_t *p4est, GhostSession &session)
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


static void AcceptNumericalSolution(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		HydroCallbacks::quadrant_accept_center_solution_callback, 
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
		IOCallbacks::quadrant_total_energy_error_callback, 
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




static void ComputeCornerAndEdgeForce(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          
		(void*)p4est_data,   
		HydroCallbacks::quadrant_compute_corner_force_callback, 
		NULL,
#ifdef P4_TO_P8
		NULL,                  

#endif
		NULL);         
}



void FluxRelaxedResetZero(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	
	p4est_iterate(p4est,
		NULL,          
		NULL,   
		HydroCallbacks::quadrant_flux_relaxed_reset_callback, 
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

		RiemannPhases::run_iteration(p4est, session,
			MatrixAssemble, ComputeCornerNodeVelocity,
			ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes);
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
		quadrant_whether_allowing_coarsening_from_corner_callback);
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
		AMRCallbacks::quadrant_edge_minmod_estimate_callback,
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
		quadrant_corner_minmod_estimate_callback);


	p4est_iterate(p4est,
		NULL,
		(void *)p4est_data,
		AMRCallbacks::quadrant_cell_minmod_estimate_callback,
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
		IOCallbacks::quadrant_write_distance_profiles_callback,
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
		IOCallbacks::quadrant_copy_variable_to_array_callback,
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
						AMRController::execute_amr(p4est, ghost_session,
				recursive, allowed_level, callbackorphans,
				Lagrangian_refine_err_estimate, Lagrangian_coarsen_err_estimate,
				Lagrangian_replace_quads, set_allowing_coarsening_tag,
				StatTotalEnergyError);
		}
		//StatGlobalFieldChecksum(p4est, "Checkpoint 2: AMR");


		if (p4est_data->current_step &&
			!(p4est_data->current_step%p4est_data->repartition_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)
		{
			AMRController::execute_partition(p4est, ghost_session, allowcoarsening);
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
			Diagnostics::check_state_invariants(p4est, 1);
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
	double global_id = MeshAdapter::global_sfc_id(p4est, local_id);
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
		Diagnostics::check_state_invariants(p4est, 0);
	}

	// Test call to the debug VTU output
	//IOAlgorithm::p4est_debug_output_vtu(p4est, "output/debug_checkpoint", 0, 0);

	const SimulationModel::SimulationClock startup_clock =
		p4est_data->simulation_clock();
	Simulation::run(p4est,
		startup_clock.start_time,
		startup_clock.end_time,
		advance_time_step);

								   
	p4est_destroy(p4est);
	p4est_connectivity_destroy(conn);

	sc_finalize();
	mpiret = sc_MPI_Finalize();
	SC_CHECK_MPI(mpiret);
	return 0;
}