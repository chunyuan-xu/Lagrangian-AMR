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
	// Isolated causality contrast: select the existing discretization before
	// freezing startup_config. Unset preserves the historical Rotated path.
	const char *riemann_form = getenv("AMR_RIEMANN_FORM");
	if (riemann_form) {
		SC_CHECK_ABORT(riemann_form[0] && !riemann_form[1] &&
			(riemann_form[0] == '0' || riemann_form[0] == '1'),
			"AMR_RIEMANN_FORM must be 0=GridAligned or 1=Rotated");
		ctx.solver_type = riemann_form[0] - '0';
	}
	P4EST_GLOBAL_PRODUCTIONF("RIEMANN_FORM selected=%d name=%s\n",
		ctx.solver_type, ctx.solver_type == 0 ? "GridAligned" : "Rotated");
	SC_CHECK_ABORT(ctx.has_valid_simulation_settings(),
		"Invalid simulation configuration or clock settings");

	P4EST_GLOBAL_PRODUCTIONF("This is the p4est %dD demo for Lagrangian hydrodynamics\n", P4EST_DIM);

	
	p4est_connectivity_t *conn = p4est_connectivity_new_unitsquare();
	

	const SimulationModel::SimulationConfig startup_config =
		ctx.simulation_config();
	// M10.4.1: install P4estBridge as the p4est->user_pointer carrier.
	P4estBridge bridge;
	bridge.data = ctx;
	bridge.config = &startup_config;
	p4est_t *p4est = p4est_new_ext(mpicomm,
		conn,
		1,
		startup_config.mesh.minimum_level,
		1,
		sizeof(quad_data_t),
		Initializer::Lagrangian_init_condition,
		(void *)(&bridge));

	p4est_data_t *p4est_data = &bridge.data;

	
	int recursive = 1;
	

	int partforcoarsen = 1;

	if (startup_config.mesh.initial_annulus_mesh) {
		P4EST_GLOBAL_PRODUCTIONF(
			"[initial-annulus] center=(%.6f, %.6f), radius=[%.6f, %.6f], base=L%d, band=L%d\n",
			startup_config.mesh.initial_annulus_center_x,
			startup_config.mesh.initial_annulus_center_y,
			startup_config.mesh.initial_annulus_inner_radius,
			startup_config.mesh.initial_annulus_outer_radius,
			startup_config.mesh.minimum_level,
			startup_config.mesh.initial_annulus_level);
		p4est_refine_ext(p4est, 1,
			startup_config.mesh.initial_annulus_level,
			AMRAgorithm::InitialAnnulusRefineErrorEstimate,
			Initializer::Lagrangian_init_condition, NULL);
		p4est_balance_ext(p4est, P4EST_CONNECT_CORNER,
			Initializer::Lagrangian_init_condition, NULL);
	}
	else {
		p4est_balance(p4est, P4EST_CONNECT_CORNER,
			Initializer::Lagrangian_init_condition);
	}
	p4est_partition(p4est, partforcoarsen, NULL);
    if(CompressionExperiment::enabled()) {
        CompressionExperiment::update(p4est);
        p4est_refine_ext(p4est,1,p4est_data->max_level,CompressionExperiment::initial_refine,Initializer::Lagrangian_init_condition,NULL);
        p4est_balance_ext(p4est,P4EST_CONNECT_CORNER,Initializer::Lagrangian_init_condition,NULL);
        p4est_partition(p4est,partforcoarsen,NULL);
        P4EST_GLOBAL_PRODUCTIONF("COMPRESSION_INIT threshold=%.17g runtime_threshold=%.17g speed=%.17g n=%lld\n",CompressionExperiment::initial_threshold(),CompressionExperiment::threshold(),CompressionExperiment::speed(),(long long)p4est->global_num_quadrants);
    }
	if (startup_config.mesh.initial_annulus_mesh) {
		P4EST_GLOBAL_PRODUCTIONF(
			"[initial-annulus] frozen topology ready: %lld global quadrants\n",
			(long long) p4est->global_num_quadrants);
	}

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
		startup_clock.end_time);

								   
	p4est_destroy(p4est);
	p4est_connectivity_destroy(conn);

	sc_finalize();
	mpiret = sc_MPI_Finalize();
	SC_CHECK_MPI(mpiret);
	return 0;
}
