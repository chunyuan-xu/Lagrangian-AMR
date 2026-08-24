#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "core/trace.h"
#include "mesh/ghost_session.h"
#include "amr/amr_controller.h"
#include "amr/amr_callbacks.h"
#include "hydro/hydro_controller.h"
#include "io/io_callbacks.h"
#include "io/output_stamp.h"
#include "diagnostics/ghost_exchange_observer.h"
#include "diagnostics/memory_probe_observer.h"
#include "diagnostics/memory_probe_output.h"
#include "diagnostics/state_invariant_checker.h"

// M7.4: Simulation — high-level orchestration. main.cpp only performs
// MPI/p4est/config/mesh setup, then forwards to Simulation::run with the
// full hydro+AMR time-step driver.

namespace Simulation {

// M9.4.1: full time-step driver (hydro + AMR loop), moved from main.cpp.
void advance_time_step(p4est_t * p4est, double start_time, double end_time)
{
	double              t = start_time;
	double              dt = 0.;
	GhostSession ghost_session;
	p4est_data_t		*p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	Diagnostics::MemoryProbeOwner memory_probe(&p4est_data->current_step);
	memory_probe.bind(ghost_session);
	int					recursive = 0;
	int					allowed_level = p4est_data->max_level;
	int					callbackorphans = 0;
	int					allowcoarsening = 1;


	if (memory_probe.enabled()) {
		Diagnostics::initialize_selected(
			ghost_session, p4est, P4EST_CONNECT_FULL);
	}
	else {
		ghost_session.initialize(p4est, P4EST_CONNECT_FULL);
	}

	for (t = start_time; t < end_time; t += p4est_data->delta_time)
	{
		p4est_data->current_step += 1;
		trace_target_snapshot(p4est, "STEP_BEGIN");
		//IOCallbacks::StatGlobalFieldChecksum(p4est, "Checkpoint 1: Start time loop");
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
						if (memory_probe.enabled()) {
						    memory_probe.context()->origin = Diagnostics::ExchangeOrigin::Rebuild;
						}
						AMRController::execute_amr(p4est, ghost_session,
				recursive, allowed_level, callbackorphans,
				AMRCallbacks::Lagrangian_refine_err_estimate, AMRCallbacks::Lagrangian_coarsen_err_estimate,
				AMRCallbacks::Lagrangian_replace_quads, AMRCallbacks::set_allowing_coarsening_tag,
				IOCallbacks::StatTotalEnergyError);
		}
		//IOCallbacks::StatGlobalFieldChecksum(p4est, "Checkpoint 2: AMR");


		if (p4est_data->current_step &&
			!(p4est_data->current_step%p4est_data->repartition_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)
		{
			AMRController::execute_partition(p4est, ghost_session, allowcoarsening);
		}


		if (ghost_session.empty())
		{
			if (memory_probe.enabled()) {
				memory_probe.context()->origin = Diagnostics::ExchangeOrigin::Rebuild;
				Diagnostics::initialize_selected(
					ghost_session, p4est, P4EST_CONNECT_FULL);
			}
			else {
				ghost_session.initialize(p4est, P4EST_CONNECT_FULL);
			}
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

		if (memory_probe.enabled()) {
			memory_probe.context()->origin = Diagnostics::ExchangeOrigin::Ordinary;
			Diagnostics::exchange_selected(ghost_session, p4est);
		}
		else {
			ghost_session.exchange();
		}


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
	if (memory_probe.enabled()) {
		Diagnostics::write_memory_high_water_rank_output(
			p4est->mpirank, p4est->mpisize, *memory_probe.context());
	}
	ghost_session.destroy();
}

inline void run(p4est_t *p4est, double start_time, double end_time)
{
	advance_time_step(p4est, start_time, end_time);
}

} // namespace Simulation
