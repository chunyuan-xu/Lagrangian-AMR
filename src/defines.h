#pragma once
#include <fstream>
#include <string>
#include "io/config_parser.h"
#include "core/simulation_config.h"
#include "Variable.h"

enum ProblemNo
{
	SedovPolar,
	SedovCartesian,
	Sedov1DCartesian,
	NohPolar,
	NohCartesian,
	Saltzman,
	SodPolar,
	SodCartesian,
	TriplePoint,
	TwoDimRiemann,
	TaylorGreen,
};

enum RefineCriteria
{
	DensityGradient,
	PressureGradient,
	DensityValue,
	PressureValue,
	VorticityValue,
	Distance,
};


struct CGlobal_grid_info
{
	int global_nx;
	int global_ny;
	double tree_width;
	double tree_height;
	CGlobal_grid_info()
	{
		global_nx = 1;
		global_ny = 1;
		tree_width = 1.;
		tree_height = 1.;
	}
};

// M10: p4est_data_t is the legacy god-object acting as p4est->user_pointer.
// It mixes read-only configuration, run-time state, and (formerly) IO
// handles. M10 progressively dismantles it:
//   M10.1.1  - ofstream handles removed -> IOCallbacks file management
//   M10.3.1a - total_energy_* sums removed -> IOCallbacks::ReductionContext
//   M10.2.1  - config fields (coord_type/Scheme_type/...) treated as
//              read-only; simulation_config() snapshots them into
//              SimulationConfig (core/simulation_config.h)
//   M10.4.1  - current_time/step/dt run-time state migrate to
//              SimulationClock via P4estBridge; p4est_data_t shrinks to a
//              thin compatibility carrier
// Until M10.4.1 the remaining fields below stay POD and are only touched
// through p4est_data_t (kept POD so MPI/p4est byte-copy semantics hold).
struct p4est_data_t {
	enum MyCoordType
	{
		plane, cylinder
	};
	int coord_type;
	CGlobal_grid_info m_grid_info;
	enum MySchemeType
	{ControlVolume, AreaWeighted};
	int Scheme_type;
	enum RiemannSolver
	{
		GridAligned, Rotated
	};
	int solver_type;
	enum SchemeOrder
	{first_order, second_order};

	enum CoarseningEnum
	{NotCoarsenedJustNow, CoarsenedJustNow, CoarsingAllowed, CoarsingNotAllowed};

	enum RefiningEnum
	{RefiningAllowed, RefiningNotAllowed, MustRefing};

	enum DistanceProfileType
	{
		xType, yType, radiusType,
	};

	enum center_type
	{
		average, integrated,
	};

	int accuracy;

	int which_case;

	int LeftBoun;
	int RightBoun;
	int BottomBoun;
	int TopBoun;
	double LeftBounVal;
	double RightBounVal;
	double BottomBounVal;
	double TopBounVal;

	double start_time;
	double end_time;
	double refine_err;
	double coarsen_error;
	double m_cfl;
	double current_time;
	double initial_dt;
	double refine_coarsen_time;
	
	double shock_velocity;
	double used_dt;
	double local_dt;  
	double delta_time;  
	double dt_iter;   
	double max_dt;    
	bool equal_dt;    
	int current_step;  
	int max_time_step; 
	int refine_coarsen_enum;    
	int minus_level;   
	int max_level;     
	int  refine_period;  
	int repartition_period; 
	int last_output_index; 
	int write_interval_step; 
	int profiletype;  
	int children_center_type;  
	int x_tree_number;  
	int y_tree_number;  
	double write_interval_time;
	// M10.3.1a: total_energy_cur/lag/init moved to IOCallbacks::ReductionContext.
	double volume_varation_torelarion;
	double dt_increase_percent;
	// M10.1.1: IO handles (EnergyFile/DistanceFile/ErrorFile) removed —
	// moved to IOCallbacks::energy_error_file()/distance_profile_file().

	p4est_data_t()
	{
		which_case = ProblemNo::SedovCartesian;
		end_time = 1.;
		x_tree_number = 1;
		y_tree_number = 1;
		refine_coarsen_enum = RefineCriteria::DensityGradient;

		local_dt = 100000.;
		delta_time = 1e-5;
		dt_iter = 0.0;
		used_dt = 0.0;
		refine_coarsen_time = 0.0;
		minus_level = 4;
		max_level = 7;
		
		
		refine_err = 1.;
		coarsen_error = 0.8;
		refine_period = 4;
		repartition_period = 8;
		write_interval_time = 0.1;
		write_interval_step = 100;

		shock_velocity = 1.0 / 3.0;
		children_center_type = center_type::average;
		profiletype = DistanceProfileType::radiusType;
		coord_type = plane;
		Scheme_type = ControlVolume;
		solver_type = Rotated;
		accuracy = first_order;
		LeftBoun = -1;
		RightBoun = -1;
		BottomBoun = -1;
		TopBoun = -1;
		LeftBounVal = 0.;
		RightBounVal = 0.;
		BottomBounVal = 0.;
		TopBounVal = 0.;
		current_time = 0.;
		start_time = 0.0;
		m_cfl = 0.1;
		initial_dt = 1e-4;
		max_dt = 1e-3;
		equal_dt = false;
		current_step = 0;
		max_time_step = 8000;

		volume_varation_torelarion = 0.01;
		dt_increase_percent = 1.001;
		last_output_index = -1;
		// M10.1.1: EnergyError.plt / DistanceProfiles.plt / ErrorFile.txt
		// opens moved to IOCallbacks lazy file-handle management.
		// M10.3.1a: total_energy_* initialized in IOCallbacks::ReductionContext.
	}

	SimulationModel::SimulationConfig simulation_config() const {
		return SimulationModel::SimulationConfig{
			which_case,
			start_time,
			end_time,
			max_time_step,
			SimulationModel::MeshConfig{
				m_grid_info.global_nx,
				m_grid_info.global_ny,
				m_grid_info.tree_width,
				m_grid_info.tree_height,
				x_tree_number,
				y_tree_number,
				minus_level,
				max_level,
				refine_coarsen_enum,
				refine_err,
				coarsen_error,
				refine_period,
				repartition_period,
				refine_coarsen_time},
			SimulationModel::SolverConfig{
				coord_type,
				Scheme_type,
				solver_type,
				accuracy,
				m_cfl,
				initial_dt,
				max_dt,
				equal_dt,
				volume_varation_torelarion,
				dt_increase_percent},
			SimulationModel::OutputConfig{
				write_interval_time,
				write_interval_step,
				profiletype}};
	}

	SimulationModel::SimulationClock simulation_clock() const {
		return SimulationModel::SimulationClock{
			start_time,
			end_time,
			current_time,
			delta_time,
			dt_iter,
			used_dt,
			current_step,
			max_time_step};
	}

	void load_from_config(const IOAlgorithm::ConfigParser& cfg) {
		if (cfg.HasKey("which_case")) {
			std::string case_str = cfg.GetString("which_case", "");
			if (case_str == "SedovPolar") which_case = ProblemNo::SedovPolar;
			else if (case_str == "SedovCartesian") which_case = ProblemNo::SedovCartesian;
			else if (case_str == "Sedov1DCartesian") which_case = ProblemNo::Sedov1DCartesian;
			else if (case_str == "NohPolar") which_case = ProblemNo::NohPolar;
			else if (case_str == "NohCartesian") which_case = ProblemNo::NohCartesian;
			else if (case_str == "Saltzman") which_case = ProblemNo::Saltzman;
			else if (case_str == "SodPolar") which_case = ProblemNo::SodPolar;
			else if (case_str == "SodCartesian") which_case = ProblemNo::SodCartesian;
			else if (case_str == "TriplePoint") which_case = ProblemNo::TriplePoint;
			else if (case_str == "TwoDimRiemann") which_case = ProblemNo::TwoDimRiemann;
			else if (case_str == "TaylorGreen") which_case = ProblemNo::TaylorGreen;
			else which_case = cfg.GetInt("which_case", which_case); 
		}
		if (cfg.HasKey("start_time")) {
			start_time = cfg.GetDouble("start_time", start_time);
			current_time = start_time;
		}
		if (cfg.HasKey("end_time")) end_time = cfg.GetDouble("end_time", end_time);
		if (cfg.HasKey("delta_time")) delta_time = cfg.GetDouble("delta_time", delta_time);
		if (cfg.HasKey("refine_coarsen_enum")) refine_coarsen_enum = cfg.GetInt("refine_coarsen_enum", refine_coarsen_enum);
		if (cfg.HasKey("minus_level")) minus_level = cfg.GetInt("minus_level", minus_level);
		if (cfg.HasKey("max_level")) max_level = cfg.GetInt("max_level", max_level);
		if (cfg.HasKey("refine_err")) refine_err = cfg.GetDouble("refine_err", refine_err);
		if (cfg.HasKey("coarsen_error")) coarsen_error = cfg.GetDouble("coarsen_error", coarsen_error);
		if (cfg.HasKey("refine_period")) refine_period = cfg.GetInt("refine_period", refine_period);
		if (cfg.HasKey("refine_coarsen_time")) refine_coarsen_time = cfg.GetDouble("refine_coarsen_time", refine_coarsen_time);
		if (cfg.HasKey("write_interval_time")) write_interval_time = cfg.GetDouble("write_interval_time", write_interval_time);
		if (cfg.HasKey("write_interval_step")) write_interval_step = cfg.GetInt("write_interval_step", write_interval_step);
		if (cfg.HasKey("max_time_step")) max_time_step = cfg.GetInt("max_time_step", max_time_step);
	}

	bool has_valid_simulation_settings() const {
		return SimulationModel::valid(simulation_config()) &&
			SimulationModel::valid(simulation_clock());
	}
};


struct CPointBounInfo
{
	enum BouDTY {Inner, Velo, Wall, Symmetry, Free, Press};
	int enumType;  
	double Val; 
	CDoubleVector Ncp; 
	double Lcp; 
	CDoubleVector delta_u_cp;   
	CDoubleVector Uc_cur;  
	double Zc;

	CPointBounInfo()
	{
		enumType = -1;
		Val = 0.;
		delta_u_cp = CDoubleVector(0., 0.);
		Zc = 0.;
	}
};


struct CPoint_data_t
{
	
	bool IsHanging;  
	bool AddDiss; 
	
	
	CPointBounInfo TwoBouns[2];
	CPointBounInfo BounParent;
	CDoubleVector master_coord_relaxed[2];  
	CDoubleVector hanging_coord;  
	CDoubleVector velo_lag;  
	double PI_hanging;
	double pi_constrained_parent;
	bool add_dissipation_child1;
	bool add_dissipation_child2;
	bool add_dissipation_parent;
	CDoubleMatrix MatrixP;   
	CDoubleVector RHS;       
};


struct CHalf_edge_data
{
	enum cside{plus, minus}; 
	double Rcp;  
	double Lcp;  
	CDoubleVector Ncp;  
	double Zcp;
	CDoubleVector delta_u_cp;
	CDoubleVector Uc_cur;
	double pi;
	enum BounDTY {Inner, Velo, Wall, Symmetry, Free, Press};
	int enumBYD; 
	double BYDVal; 
	bool is_hanging; 
	int which_face; 
	CHalf_edge_data()
	{
		Rcp = 1.;
		Lcp = 0.;
		Ncp = CDoubleVector(1., 0.);
		Zcp = 0.;
		enumBYD = BounDTY::Inner;
		is_hanging = false;
	}
};


struct CCorner_data
{
	CHalf_edge_data hdata[2];   
};

struct CEdge_data
{
	enum enumET{Inner, Velo, Wall, Symmetry, Free, Press};
	int EdgeType;
};


struct ParentBounInfo
{
	bool IsParentChildBoun;   
	bool addDiss;
	CDoubleVector Ncp[2];   
	double ParentPIStar;   
	CDoubleVector FluxRelaxed;
	double Lcp[2];
	double Zcp;

	CDoubleVector Hanging_velocity;  
	ParentBounInfo()
	{
		IsParentChildBoun = false;    
	}
};


typedef struct quad_data
{
	
	
	enum EnumCorner
	{
		LEFTBOTTOM, LEFTUP, RIGHTUP, RIGHTBOTTOM   
	};
	CCorner_data m_cndata[CNDIM];   

	enum EnumEdge
	{
		LEFT, RIGHT, BOTTOM, UP
	};
	CEdge_data m_edata[CNDIM];   

	CPoint_data_t points[CNDIM];  
	double init_node_coords[CNDIM][P4EST_DIM];  
	
	ParentBounInfo m_pc_edge_data[CNDIM]; 

	CVariable m_vara;    

	int face_neighbors[2 * CNDIM];    
	int face_num;
}quad_data_t;  


enum p4est_enum_corner
{
	left_bottom, right_bottom, left_up, right_up,
};


typedef struct {
	sc_array_t *pressure_array;
	sc_array_t *density_array;
	sc_array_t *temperature_array;
	sc_array_t *internal_energy_array;

	sc_array_t *coordx;
	sc_array_t *coordy;
	sc_array_t *velox;
	sc_array_t *veloy;
}vtu_cell_data_t;

typedef struct {
	sc_array_t *global_sfc_id_array;
	sc_array_t *pressure_array;
	sc_array_t *density_array;
	sc_array_t *internal_energy_array;

	sc_array_t *pressure_c0_array;
	sc_array_t *pressure_c1_array;
	sc_array_t *pressure_c2_array;
	sc_array_t *pressure_c3_array;

	sc_array_t *velou_c0_array;
	sc_array_t *velou_c1_array;
	sc_array_t *velou_c2_array;
	sc_array_t *velou_c3_array;

	sc_array_t *velov_c0_array;
	sc_array_t *velov_c1_array;
	sc_array_t *velov_c2_array;
	sc_array_t *velov_c3_array;
}debug_vtu_cell_data_t;

typedef struct {
	void *p4est_data;
	void *quad_data;
}my_user_data_t;
