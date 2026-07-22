#pragma once
#include <fstream>
#include <string>
#include "io/config_parser.h"
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

//ȫ��������Ϣ�ṹ
struct CGlobal_grid_info
{
	int global_nx;//ȫ��������x�����������
	int global_ny;//ȫ��������y�����������
	double tree_width;//ÿ�����Ŀ���
	double tree_height;//ÿ�����ĸ߶�
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

	int LeftBoun;//��߽�
	int RightBoun;//�ұ߽�
	int BottomBoun;//�±߽�
	int TopBoun;//�ϱ߽�
	double LeftBounVal;//��߽綨ѹ��/�ٶ�ֵ
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
	double used_dt;//��һ��ʱ�䲽��
	double local_dt;  //��ǰ����ʱ�䲽��
	double delta_time;  //����ʱ�䲽��
	double dt_iter;   //������ʱ�䲽��
	double max_dt;    //���������ʱ�䲽��
	bool equal_dt;    //�Ƿ�ʱ�䲽������
	int current_step;  //��ǰʱ�䲽
	int max_time_step; //���ʱ�䲽��
	int refine_coarsen_enum;    //�����������ж��Ƿ���Ҫ����
	int minus_level;   //��Сϸ���㼶
	int max_level;     //���ϸ���㼶
	int  refine_period;  //ϸ�����
	int repartition_period; //�ػ��ּ��
	int last_output_index; //�ϴ��������
	int write_interval_step; //д�ļ��������
	int profiletype;  //
	int children_center_type;  //
	int x_tree_number;  //ǰ������������ʹ�ã�ָ��x����p4est������Ŀ
	int y_tree_number;  
	double write_interval_time;//д�ļ�ʱ����
	double total_energy_lag;
	double total_energy_cur;
	double total_energy_init;
	double volume_varation_torelarion;/*����ʱ�䲽����������仯*/
	double dt_increase_percent;//ʱ�䲽��������
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
		max_time_step = 1000;

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

	void load_from_config(const IOAlgorithm::ConfigParser& cfg) {
		if (cfg.HasKey("which_case")) which_case = cfg.GetInt("which_case", which_case);
		if (cfg.HasKey("start_time")) start_time = cfg.GetDouble("start_time", start_time);
		if (cfg.HasKey("end_time")) end_time = cfg.GetDouble("end_time", end_time);
		if (cfg.HasKey("delta_time")) delta_time = cfg.GetDouble("delta_time", delta_time);
		if (cfg.HasKey("refine_coarsen_enum")) refine_coarsen_enum = cfg.GetInt("refine_coarsen_enum", refine_coarsen_enum);
		if (cfg.HasKey("minus_level")) minus_level = cfg.GetInt("minus_level", minus_level);
		if (cfg.HasKey("max_level")) max_level = cfg.GetInt("max_level", max_level);
		if (cfg.HasKey("refine_err")) refine_err = cfg.GetDouble("refine_err", refine_err);
		if (cfg.HasKey("coarsen_error")) coarsen_error = cfg.GetDouble("coarsen_error", coarsen_error);
		if (cfg.HasKey("refine_period")) refine_period = cfg.GetInt("refine_period", refine_period);
		if (cfg.HasKey("write_interval_time")) write_interval_time = cfg.GetDouble("write_interval_time", write_interval_time);
		if (cfg.HasKey("write_interval_step")) write_interval_step = cfg.GetInt("write_interval_step", write_interval_step);
	}
};//ɭ������


/*�Զ������ݽṹ���߽���Ϣ*/
struct CPointBounInfo
{
	enum BouDTY {Inner, Velo, Wall, Symmetry, Free, Press};
	int enumType;  //�߽�����
	double Val; //�߽�ֵ
	CDoubleVector Ncp; //�߽絥λ�ⷨ����
	double Lcp; //�߽�ߵĳ���
	CDoubleVector delta_u_cp;   //uc-up
	CDoubleVector Uc_cur;  //���������ٶ�
	double Zc;

	CPointBounInfo()
	{
		enumType = -1;
		Val = 0.;
		delta_u_cp = CDoubleVector(0., 0.);
		Zc = 0.;
	}
};

/*�Զ������ݽṹ���洢�ڵ�����*/
struct CPoint_data_t
{
	//bool IsBoundary;
	bool IsHanging;  /*�Ƿ������ҽڵ�*/
	bool AddDiss; /*�Ƿ����Ӻ�ɢ*/
	/*һ���ڵ�����������߽�ߣ�������ѹ��+ѹ����ѹ��+�ٶȣ��ٶ�+�ٶȵ����ͱ߽�*/
	/*TwoBoun[0]��TwoBoun[1]�ֱ�洢�߽�ߵ���Ϣ*/
	/*���Ϊ�ڲ��㣬TwoBouns��enumBou��Ϊ�ڲ��߽�����*/
	CPointBounInfo TwoBouns[2];
	CPointBounInfo BounParent;
	CDoubleVector master_coord_relaxed[2];  //�ɳ�Լ�������£������˵������
	CDoubleVector hanging_coord;  //nʱ�����������
	CDoubleVector velo_lag;  //n+1ʱ�̽ڵ��ٶ�
	double PI_hanging;
	double pi_constrained_parent;
	bool add_dissipation_child1;//������1�Ƿ����Ӻ�ɢ
	bool add_dissipation_child2;//������2�Ƿ����Ӻ�ɢ
	bool add_dissipation_parent;//�������Ƿ����Ӻ�ɢ
	CDoubleMatrix MatrixP;   //�ڵ�����������ڵ��ٶ�
	CDoubleVector RHS;       //�����Ҷ���
};

/*�Զ������ݽṹ���ڵ���Χ�İ��*/
/*��ͼ�����ڵ�p��Ӧ���������half_edge���ֱ���Plus��Minus��ʾ*/
/*     Minus       */
/*-----------------p*/
/*                 |*/
/*                 |*/
/*                 |Plus*/
/*                 |*/
/*                 |*/
struct CHalf_edge_data
{
	enum cside{plus, minus}; //corner_side,����plus��minus��������ͼ
	double Rcp;  //����Ȩ������
	double Lcp;  //��߳���
	CDoubleVector Ncp;  //�ⷨ����
	double Zcp;
	CDoubleVector delta_u_cp;
	CDoubleVector Uc_cur;
	double pi;
	enum BounDTY {Inner, Velo, Wall, Symmetry, Free, Press};//�߽����ͣ��ڲ����ٶȡ��̱ڡ��Գ��ᡢ���ɡ�ѹ��
	int enumBYD; //�߽�����
	double BYDVal; //���߽�ֵ�����ٶȡ�ѹ����
	bool is_hanging; //������������Ƿ���ڴ֡�ϸ����������ڣ���Ϊ���ұ�
	int which_face; //��quad����������
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

/*�Զ������ݽṹ���������*/
/*��ͼ�����ڵ�P��Ӧ���������half_edge���ֱ���Plus��Minus��ʾ*/
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
	CHalf_edge_data hdata[2];   //����������
};

struct CEdge_data
{
	enum enumET{Inner, Velo, Wall, Symmetry, Free, Press};
	int EdgeType;
};

/*��������߽�ߣ���¼������ߵ���Ϣ*/
struct ParentBounInfo
{
	bool IsParentChildBoun;   //������Ƿ��Ǹ��ӱ߽硣���ǣ��ñ��Ǹ�����߽��
	bool addDiss;
	CDoubleVector Ncp[2];   //�����ߵ��ⷨ����
	double ParentPIStar;   //������ߵ�ѹǿ
	CDoubleVector FluxRelaxed;
	double Lcp[2];
	double Zcp;

	CDoubleVector Hanging_velocity;  //�����ٶ�
	ParentBounInfo()
	{
		IsParentChildBoun = false;    //Ĭ��Ϊ�Ǹ�������߽�
	}
};

/*�Զ������ݽṹ���洢quadrant����*/
typedef struct quad_data
{
	/*�û��Զ�����Ǳ��*/
	/*CNDIM = 0~3*/
	/*1--------2*/
	/*----------*/
	/*----------*/
	/*----------*/
	/*0--------3*/
	enum EnumCorner
	{
		LEFTBOTTOM, LEFTUP, RIGHTUP, RIGHTBOTTOM   /*���½ǣ����Ͻǣ����Ͻǣ����½�*/
	};
	CCorner_data m_cndata[CNDIM];   //�����������

	enum EnumEdge
	{
		LEFT, RIGHT, BOTTOM, UP
	};
	CEdge_data m_edata[CNDIM];   //���������

	CPoint_data_t points[CNDIM];  //�ĸ���Ƕ�Ӧ�ĸ��ڵ����ݣ����ڱ߽紦��
	double init_node_coords[CNDIM][P4EST_DIM];  //��ʼ����
	
	ParentBounInfo m_pc_edge_data[CNDIM]; //parent-children������

	CVariable m_vara;    //����������

	int face_neighbors[2 * CNDIM];    //���ڵ��浥Ԫquad_id
	int face_num;
}quad_data_t;  //Ҷ����������

/*p4estĬ�ϵ���Ǳ��*/
/*2--------3*/
/*----------*/
/*----------*/
/*----------*/
/*0--------1*/
enum p4est_enum_corner
{
	left_bottom, right_bottom, left_up, right_up,
};

//�û����ݽṹ���洢��������
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
