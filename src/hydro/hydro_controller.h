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
#include "physics/stage_policy.h"
#include "solver/solver_gate.h"
#include "init/initializer.h"
#include "io/io_callbacks.h"

// M9.2: HydroController — high-level hydro orchestration shells stripped from main.cpp.

namespace HydroController {

void FluxRelaxedResetZero(p4est_t *p4est);
void MatrixAssemble(p4est_t *p4est, GhostSession &session);
void ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est_t *p4est, GhostSession &session);
void ComputeCornerNodeVelocity(p4est_t *p4est, GhostSession &session);
void ComputeCornerAndEdgeForce(p4est_t *p4est);
void MirrorNodalBoundary(p4est_t *p4est);
void MirrorNodalGeometry(p4est_t *p4est);



void 
predict_timestep(p4est_t *p4est)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;


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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;

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
	p4est_data_t	*p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;

	
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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;


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
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
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

void MirrorNodalBoundary(p4est_t *p4est)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		HydroCallbacks::quadrant_mirror_boundary_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

void MirrorNodalGeometry(p4est_t *p4est)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		HydroCallbacks::quadrant_mirror_face_geometry_callback,
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

// M9.2.3: single-stage hydro advance (M9.2.2 skipped item). Orchestrates
// boundary, half-time, corner matrix/velocity, divergence, coordinate,
// and conservative-update phases with trace/checksum diagnostics.
void advance_single_stage(p4est_t * p4est, GhostSession &session)
{
	p4est_data_t	*p4est_data = &((P4estBridge *)p4est->user_pointer)->data;


	Initializer::get_boundary_from_p4est(p4est);
	p4est_data->dt_iter =
		StagePolicy::timestep_scale(0) * p4est_data->delta_time;

	HydroController::CalculateHalfTimeVariable(p4est);
		trace_target_snapshot(p4est, "AFTER_HALF");
		//IOCallbacks::StatGlobalFieldChecksum(p4est, "Checkpoint 3: Predict");


		HydroController::CalculateCornerRcpLcpNcp(p4est);
		trace_target_snapshot(p4est, "AFTER_RCP");
		HydroController::MirrorNodalBoundary(p4est);
		session.exchange();


		AMRCallbacks::Get_AMR_BDY_info(p4est, session);
		trace_target_snapshot(p4est, "AFTER_AMR_BDY");
		HydroController::MirrorNodalGeometry(p4est);
		session.exchange();


		const SolverGate::CoordinateType coordinate_type =
			SolverGate::coordinate_type_from_legacy(p4est_data->coord_type);
		const SolverGate::SolverType solver_type =
			SolverGate::solver_type_from_legacy(p4est_data->solver_type);
		if (SolverGate::should_run_riemann(coordinate_type, solver_type))
		{
			HydroController::RiemannSolver(p4est, session);
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

		//IOCallbacks::StatGlobalFieldChecksum(p4est, "Checkpoint 4: RiemannSolver");


		HydroController::ComputeDivergence(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 3: Divergence");


		HydroController::ComputeCoordinate(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 4: Coordinate");


		HydroController::UpdateDensity(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 5: Density");


		HydroController::UpdateMomentumEquation(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 6: Momentum");


		HydroController::ComputeWork(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 7: Work");


		HydroController::UpdateEnergyEquation(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 8: EnergyEq");


		HydroController::UpdateEquationOfState(p4est);
		if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 9: EOS");


	HydroController::ComputeSoundSpeed(p4est);
	if (checksum_trace_enabled()) IOCallbacks::StatGlobalFieldChecksum(p4est, "SubStep 10: SoundSpeed");
	//IOCallbacks::StatGlobalFieldChecksum(p4est, "Checkpoint 5: Update");
	p4est_data->used_dt = p4est_data->delta_time;
}

// M9.2.4: MUSCL gradient estimation shell and PreProcess (default tags).
void
Gradient_estimate(p4est_t *p4est, GhostSession &session)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	GhostCallbackContext callback_context = { &session };

	p4est_iterate(p4est,
		session.get(),
		(void *)session.data(),
		HydroCallbacks::quadrant_set_gradient_zero_estimate_callback,
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
		HydroCallbacks::quadrant_corner_minmod_estimate_callback);


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

void PreProcess(p4est_t *p4est, GhostSession &session)
{

	HydroController::Gradient_estimate(p4est, session);

	AMRCallbacks::set_default_coarsening_tag(p4est);


	AMRCallbacks::set_default_refining_tag(p4est);
}

} // namespace HydroController
