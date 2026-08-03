#pragma once

#include <cmath>

namespace SimulationModel {

struct MeshConfig {
	int global_nx;
	int global_ny;
	double tree_width;
	double tree_height;
	int x_tree_number;
	int y_tree_number;
	int minimum_level;
	int maximum_level;
	int refine_criterion;
	double refine_error;
	double coarsen_error;
	int refine_period;
	int repartition_period;
	double refine_coarsen_time;
};

struct SolverConfig {
	int coordinate_type;
	int scheme_type;
	int solver_type;
	int accuracy;
	double cfl;
	double initial_timestep;
	double maximum_timestep;
	bool equal_timestep;
	double volume_variation_tolerance;
	double timestep_increase_factor;
};

struct OutputConfig {
	double write_interval_time;
	int write_interval_step;
	int profile_type;
};

struct SimulationConfig {
	int problem;
	double start_time;
	double end_time;
	int maximum_time_step;
	MeshConfig mesh;
	SolverConfig solver;
	OutputConfig output;
};

struct SimulationClock {
	double start_time;
	double end_time;
	double current_time;
	double delta_time;
	double stage_timestep;
	double used_timestep;
	int current_step;
	int maximum_time_step;
};

inline bool valid(const MeshConfig &config)
{
	return config.refine_criterion >= 0 &&
		config.refine_criterion <= 5 &&
		config.global_nx > 0 &&
		config.global_ny > 0 &&
		std::isfinite(config.tree_width) &&
		config.tree_width > 0.0 &&
		std::isfinite(config.tree_height) &&
		config.tree_height > 0.0 &&
		std::isfinite(config.refine_error) &&
		std::isfinite(config.coarsen_error) &&
		std::isfinite(config.refine_coarsen_time) &&
		config.x_tree_number > 0 &&
		config.y_tree_number > 0 &&
		config.minimum_level >= 0 &&
		config.maximum_level >= config.minimum_level &&
		config.refine_period > 0 &&
		config.repartition_period > 0;
}

inline bool valid(const SolverConfig &config)
{
	return config.coordinate_type >= 0 &&
		config.coordinate_type <= 1 &&
		config.scheme_type >= 0 &&
		config.scheme_type <= 1 &&
		config.solver_type >= 0 &&
		config.solver_type <= 1 &&
		config.accuracy >= 0 &&
		config.accuracy <= 1 &&
		std::isfinite(config.cfl) &&
		config.cfl > 0.0 &&
		std::isfinite(config.initial_timestep) &&
		config.initial_timestep > 0.0 &&
		std::isfinite(config.maximum_timestep) &&
		config.maximum_timestep > 0.0 &&
		std::isfinite(config.volume_variation_tolerance) &&
		config.volume_variation_tolerance >= 0.0 &&
		std::isfinite(config.timestep_increase_factor) &&
		config.timestep_increase_factor > 0.0;
}

inline bool valid(const OutputConfig &config)
{
	return config.profile_type >= 0 &&
		config.profile_type <= 2 &&
		std::isfinite(config.write_interval_time) &&
		config.write_interval_time > 0.0 &&
		config.write_interval_step > 0;
}

inline bool valid(const SimulationClock &clock)
{
	return std::isfinite(clock.start_time) &&
		std::isfinite(clock.end_time) &&
		std::isfinite(clock.current_time) &&
		std::isfinite(clock.delta_time) &&
		clock.end_time >= clock.start_time &&
		clock.current_time >= clock.start_time &&
		clock.delta_time > 0.0 &&
		clock.current_step >= 0 &&
		clock.maximum_time_step > 0;
}

inline bool valid(const SimulationConfig &config)
{
	return config.problem >= 0 &&
		config.problem <= 10 &&
		std::isfinite(config.start_time) &&
		std::isfinite(config.end_time) &&
		config.end_time >= config.start_time &&
		config.maximum_time_step > 0 &&
		valid(config.mesh) &&
		valid(config.solver) &&
		valid(config.output);
}

}
