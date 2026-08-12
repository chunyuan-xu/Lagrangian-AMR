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
#include "hydro/hydro_controller.h"
#include "solver/hydro_callbacks.h"
#include "io/vtk_writer.h"
#include "io/io_callbacks.h"
#include "io/config_parser.h"
#include "io/output_stamp.h"
#include "physics/timestep_reduction.h"
#include "physics/stage_policy.h"
#include "diagnostics/state_invariant_checker.h"
#include "simulation/simulation.h"
#include "init/initializer.h"
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

		
		HydroController::PreProcess(p4est, ghost_session);
		trace_target_snapshot(p4est, "AFTER_PREPROCESS");

		
		if (p4est_data->current_step && !(p4est_data->current_step%p4est_data->refine_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)

		{
						AMRController::execute_amr(p4est, ghost_session,
				recursive, allowed_level, callbackorphans,
				AMRCallbacks::Lagrangian_refine_err_estimate, AMRCallbacks::Lagrangian_coarsen_err_estimate,
				AMRCallbacks::Lagrangian_replace_quads, AMRCallbacks::set_allowing_coarsening_tag,
				IOCallbacks::StatTotalEnergyError);
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


		AMRCallbacks::refresh_after_balance(p4est, ghost_session);
		if (refresh_idempotence_check_enabled()) {
			std::vector<unsigned char> first_refresh;
			std::vector<unsigned char> second_refresh;
			AMRCallbacks::append_refresh_snapshot(p4est, ghost_session, first_refresh);
			AMRCallbacks::refresh_after_balance(p4est, ghost_session);
			AMRCallbacks::append_refresh_snapshot(p4est, ghost_session, second_refresh);
			const int local_match = first_refresh == second_refresh ? 1 : 0;
			int global_match = 0;
			sc_MPI_Allreduce(&local_match, &global_match, 1, sc_MPI_INT,
				sc_MPI_MIN, p4est->mpicomm);
			SC_CHECK_ABORT(global_match,
				"refresh_after_balance is not idempotent");
		}
		trace_target_snapshot(p4est, "AFTER_AMR_REFRESH");

		ghost_session.exchange();

		
		if (p4est_data->equal_dt == false) { HydroController::predict_timestep(p4est); }

		
		const IOAlgorithm::OutputStamp output_stamp =
			IOAlgorithm::make_pre_step_stamp(
				p4est_data->current_step, p4est_data->current_time);
		if (!(p4est_data->current_step % p4est_data->write_interval_step))
		{
			IOCallbacks::write_solution(p4est, output_stamp);
		}
		else if (current_output_index > p4est_data->last_output_index)
		{
			p4est_data->last_output_index = current_output_index;
			IOCallbacks::write_solution(p4est, output_stamp);
		}
		else if (p4est_data->current_time+ p4est_data->delta_time >= p4est_data->end_time)
		{
			IOCallbacks::write_solution(p4est, output_stamp);
		}

		
		HydroController::advance_single_stage(p4est, ghost_session);

		
		IOCallbacks::StatTotalEnergyError(p4est);

		
		HydroController::AcceptNumericalSolution(p4est);

		if (state_invariant_check_enabled()) {
			Diagnostics::check_state_invariants(p4est, 1);
		}

		p4est_data->current_time = p4est_data->current_time + p4est_data->delta_time;

		
		P4EST_GLOBAL_PRODUCTIONF("simulation_step= %d, delta_time = %.10lf, simulation_time = %.6lf \n",
			p4est_data->current_step, p4est_data->delta_time, p4est_data->current_time);
	}
	IOCallbacks::write_distance_profiles(p4est);
	ghost_session.destroy();
}

namespace IOAlgorithm {


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
		Initializer::Lagrangian_init_condition,
		(void *)(&ctx));          

	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	
	int recursive = 1;
	

	int partforcoarsen = 1;

	
	p4est_balance(p4est, P4EST_CONNECT_CORNER, Initializer::Lagrangian_init_condition);
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