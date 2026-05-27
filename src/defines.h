#pragma once
#include "Variable.h"
#include <fstream>

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

//全局网格信息结构
struct CGlobal_grid_info
{
	int global_nx;//全局网格在x方向的树数量
	int global_ny;//全局网格在y方向的树数量
	double tree_width;//每个树的宽度
	double tree_height;//每个树的高度
	CGlobal_grid_info()
	{
		global_nx = 1;
		global_ny = 1;
		tree_width = 1.;
		tree_height = 1.;
	}
};

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

	int LeftBoun;//左边界
	int RightBoun;//右边界
	int BottomBoun;//下边界
	int TopBoun;//上边界
	double LeftBounVal;//左边界定压力/速度值
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
	//
	double shock_velocity;
	double used_dt;//上一步时间步长
	double local_dt;  //当前进程时间步长
	double delta_time;  //本步时间步长
	double dt_iter;   //迭代步时间步长
	double max_dt;    //允许的最大时间步长
	bool equal_dt;    //是否定时间步长计算
	int current_step;  //当前时间步
	int max_time_step; //最大时间步数
	int refine_coarsen_enum;    //用那种物理判断是否需要加密
	int minus_level;   //最小细化层级
	int max_level;     //最大细化层级
	int  refine_period;  //细化间隔
	int repartition_period; //重划分间隔
	int last_output_index; //上次输出索引
	int write_interval_step; //写文件步数间隔
	int profiletype;  //
	int children_center_type;  //
	int x_tree_number;  //前处理生成网格使用，指定x方向p4est树的数目
	int y_tree_number;  
	double write_interval_time;//写文件时间间隔
	double total_energy_lag;
	double total_energy_cur;
	double total_energy_init;
	double volume_varation_torelarion;/*单个时间步允许的体积变化*/
	double dt_increase_percent;//时间步长增长率
	ofstream EnergyFile;
	ofstream DistanceFile;
	ofstream ErrorFile;

	p4est_data_t()
	{
		which_case = ProblemNo::SodCartesian;
		end_time = 1.;
		x_tree_number = 1;
		y_tree_number = 1;
		refine_coarsen_enum = RefineCriteria::DensityGradient;
		if (which_case == ProblemNo::NohCartesian)
		{
			end_time = 0.6;
			refine_coarsen_enum = RefineCriteria::Distance;
		}
		if (which_case == ProblemNo::TaylorGreen)
		{
			end_time = 0.5;
		}
		if (which_case == ProblemNo::NohPolar)
		{
			end_time = 0.6;
		}
		if (which_case == ProblemNo::SedovCartesian)
		{
			end_time = 1.;
		}
		if (which_case == ProblemNo::Sedov1DCartesian)
		{
			end_time = 1.;
		}
		if (which_case == ProblemNo::SedovPolar)
		{
			end_time = 1.;
		}
		if (which_case == ProblemNo::TriplePoint)
		{
			end_time = 3.;
			m_grid_info.global_nx = 70;
			m_grid_info.global_ny = 30;
			m_grid_info.tree_height = 1.;
			m_grid_info.tree_width = 1.;
		}
		if (which_case == ProblemNo::SodCartesian
			|| which_case == ProblemNo::TwoDimRiemann)
		{
			end_time = 0.2;
		}

		local_dt = 100000.;
		delta_time = 1e-5;
		refine_coarsen_time = 0.0001;
		minus_level = 4;
		max_level = 7;
		//refine_err = 10.;
		//coarsen_error = 9.9;
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
		max_time_step = 10000000;

		volume_varation_torelarion = 0.01;
		dt_increase_percent = 1.001;
		last_output_index = -1;
		EnergyFile.open("EnergyError.plt");
		EnergyFile.setf(ios::fixed, ios::floatfield);
		EnergyFile.precision(16);

		DistanceFile.open("DistanceProfiles.plt");
		DistanceFile.setf(ios::fixed, ios::floatfield);
		DistanceFile.precision(16);

		ErrorFile.open("ErrorFile.txt");
		ErrorFile.setf(ios::fixed, ios::floatfield);
		ErrorFile.precision(16);
	}
};//森林数据


/*自定义数据结构，边界信息*/
struct CPointBounInfo
{
	enum BouDTY {Inner, Velo, Wall, Symmetry, Free, Press};
	int enumType;  //边界类型
	double Val; //边界值
	CDoubleVector Ncp; //边界单位外法向量
	double Lcp; //边界边的长度
	CDoubleVector delta_u_cp;   //uc-up
	CDoubleVector Uc_cur;  //网格中心速度
	double Zc;

	CPointBounInfo()
	{
		enumType = -1;
		Val = 0.;
		delta_u_cp = CDoubleVector(0., 0.);
		Zc = 0.;
	}
};

/*自定义数据结构，存储节点数据*/
struct CPoint_data_t
{
	//bool IsBoundary;
	bool IsHanging;  /*是否是悬挂节点*/
	bool AddDiss; /*是否增加耗散*/
	/*一个节点最多右两个边界边，可能是压力+压力，压力+速度，速度+速度等类型边界*/
	/*TwoBoun[0]和TwoBoun[1]分别存储边界边的信息*/
	/*如果为内部点，TwoBouns的enumBou都为内部边界类型*/
	CPointBounInfo TwoBouns[2];
	CPointBounInfo BounParent;
	CDoubleVector master_coord_relaxed[2];  //松弛约束条件下，两个端点的坐标
	CDoubleVector hanging_coord;  //n时刻悬点的坐标
	CDoubleVector velo_lag;  //n+1时刻节点速度
	double PI_hanging;
	double pi_constrained_parent;
	bool add_dissipation_child1;//子网格1是否增加耗散
	bool add_dissipation_child2;//子网格2是否增加耗散
	bool add_dissipation_parent;//父网格是否增加耗散
	CDoubleMatrix MatrixP;   //节点矩阵，用于求解节点速度
	CDoubleVector RHS;       //方程右端项
};

/*自定义数据结构，节点周围的半边*/
/*下图表明节点p对应网格的两条half_edge，分别用Plus和Minus表示*/
/*     Minus       */
/*-----------------p*/
/*                 |*/
/*                 |*/
/*                 |Plus*/
/*                 |*/
/*                 |*/
struct CHalf_edge_data
{
	enum cside{plus, minus}; //corner_side,其中plus和minus定义如上图
	double Rcp;  //坐标权重因子
	double Lcp;  //半边长度
	CDoubleVector Ncp;  //外法向量
	double Zcp;
	CDoubleVector delta_u_cp;
	CDoubleVector Uc_cur;
	double pi;
	enum BounDTY {Inner, Velo, Wall, Symmetry, Free, Press};//边界类型：内部、速度、固壁、对称轴、自由、压力
	int enumBYD; //边界条件
	double BYDVal; //定边界值，如速度、压力等
	bool is_hanging; //半边左右两侧是否存在粗、细网格，如果存在，则为悬挂边
	int which_face; //在quad的哪条边上
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

/*自定义数据结构，网格隅角*/
/*下图表明节点P对应网格的两条half_edge，分别用Plus和Minus表示*/
/*              */
/*--------------p*/
/*--------------|*/
/*--------------|*/
/*----corner----|*/
/*--------------|*/
/*--------------|*/

/*four corners:*/
//1--plus---minus-2
//|               |
//min            plus
//|               |
//plus           min
//|               |
//0--min----plus--3
struct CCorner_data
{
	CHalf_edge_data hdata[2];   //隅角两条半边
};

struct CEdge_data
{
	enum enumET{Inner, Velo, Wall, Symmetry, Free, Press};
	int EdgeType;
};

/*父子网格边界边，记录父网格边的信息*/
struct ParentBounInfo
{
	bool IsParentChildBoun;   //网格边是否是父子边界。若是，该边是父网格边界边
	bool addDiss;
	CDoubleVector Ncp[2];   //该条边的外法向量
	double ParentPIStar;   //父网格边的压强
	CDoubleVector FluxRelaxed;
	double Lcp[2];
	double Zcp;

	CDoubleVector Hanging_velocity;  //悬点速度
	ParentBounInfo()
	{
		IsParentChildBoun = false;    //默认为非父子网格边界
	}
};

/*自定义数据结构，存储quadrant数据*/
typedef struct quad_data
{
	/*用户自定义隅角编号*/
	/*CNDIM = 0~3*/
	/*1--------2*/
	/*----------*/
	/*----------*/
	/*----------*/
	/*0--------3*/
	enum EnumCorner
	{
		LEFTBOTTOM, LEFTUP, RIGHTUP, RIGHTBOTTOM   /*左下角，左上角，右上角，右下角*/
	};
	CCorner_data m_cndata[CNDIM];   //网格隅角数据

	enum EnumEdge
	{
		LEFT, RIGHT, BOTTOM, UP
	};
	CEdge_data m_edata[CNDIM];   //网格边数据

	CPoint_data_t points[CNDIM];  //四个隅角对应四个节点数据，用于边界处理
	double init_node_coords[CNDIM][P4EST_DIM];  //初始坐标
	
	ParentBounInfo m_pc_edge_data[CNDIM]; //parent-children边数据

	CVariable m_vara;    //物理量数据

	int face_neighbors[2 * CNDIM];    //相邻的面单元quad_id
	int face_num;
}quad_data_t;  //叶子网格数据

/*p4est默认的隅角编号*/
/*2--------3*/
/*----------*/
/*----------*/
/*----------*/
/*0--------1*/
enum p4est_enum_corner
{
	left_bottom, right_bottom, left_up, right_up,
};

//用户数据结构（存储物理量）
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
	void *p4est_data;
	void *quad_data;
}my_user_data_t;
