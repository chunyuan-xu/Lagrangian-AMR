#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "mesh/ghost_session.h"
#include "mesh/ghost_context.h"
#include "amr/amr_callbacks.h"
#include "hydro/hydro_callbacks.h"
#include "solver/riemann_phases.h"
#include "solver/hydro_phases.h"
#include "solver/hydro_callbacks.h"
#include "solver/corner_solver.h"
#include "physics/corner_solve.h"
#include "physics/timestep_reduction.h"
#include "io/io_callbacks.h"

// M9.2: HydroController — high-level hydro orchestration shells stripped from main.cpp.

namespace HydroController {

void FluxRelaxedResetZero(p4est_t *p4est);
void MatrixAssemble(p4est_t *p4est, GhostSession &session);
void ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est_t *p4est, GhostSession &session);
void ComputeCornerNodeVelocity(p4est_t *p4est, GhostSession &session);
void ComputeCornerAndEdgeForce(p4est_t *p4est);



void 
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

void RiemannSolver(p4est_t * p4est, GhostSession &session)
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
		HydroCallbacks::quadrant_corner_to_point_matrix_assemble_callback);
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
		HydroCallbacks::quadrant_hanging_point_matrix_assemble_callback,
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

void ComputeCornerNodeVelocity(p4est_t * p4est, GhostSession &session)
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

void ComputeCoordinate(p4est_t * p4est)
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

void UpdateDensity(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_density_callback);
}

void UpdateMomentumEquation(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_momentum_callback);
}

void ComputeWork(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_work_callback);
}

void UpdateEnergyEquation(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_energy_callback);
}

void UpdateEquationOfState(p4est_t * p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_update_EOS_callback);
}

void AcceptNumericalSolution(p4est_t * p4est)
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

void ComputeCornerAndEdgeForce(p4est_t * p4est)
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

void ComputeDivergence(p4est_t *p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_divergence_callback);
}

void ComputeSoundSpeed(p4est_t *p4est)
{
	HydroPhases::run_volume_update(p4est, HydroPhases::quadrant_compute_soundspeed_callback);
}

} // namespace HydroController
