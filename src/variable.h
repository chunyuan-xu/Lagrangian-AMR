#pragma once
#include <vector>
#include<sc_options.h>
#include<algorithm>
using namespace std;

#ifndef p4est_ALE_const
#define CNDIM   4//�ı��������ĸ��ǵ�
#define P4EST_DIM 2  
#define fixed_iter_num 1  /*�򵥵���Ĭ�ϵ�������*/
/*�߽����Ͷ���*/
#define InnerBoundary     -1    //�ڲ��߽�
#define PressureBoundary  1     //ѹ���߽�
#define WallBoundary      2     //�̱ڱ߽�
#define VelocityBoundary  3     //�ٶȱ߽�
#define FreeBoundary      4     //���ɱ߽�
#define SymmetryBoundary  5     //�Գ���߽�
#define CircleCenterBoundary  6     //���Ĳ����߽�

/*�������Ҷ���*/
#define LeftIndex         0
#define RightIndex        1
#define BottomIndex       0
#define UpIndex           1

#define NotHanging     0
#define Hanging        1

#define m_eps           1e-12
#define m_coliner_eps   1e-13

#define blank '\\'
constexpr double pi = 3.14159265358979323846;
#endif // !p4est_ALE_const


struct CDoubleVector {
	double x;
	double y;
	CDoubleVector() :x(0), y(0)
	{}
	CDoubleVector(double ax, double ay) :x(ax), y(ay)
	{}
	bool operator==(const CDoubleVector &a) const
	{
		return((fabs(x - a.x)<1e-100) && (fabs(y - a.y)<1e-100));
	}
	bool operator!=(const CDoubleVector &a) const
	{
		return !(*this == a);
	}
	friend CDoubleVector operator+(const CDoubleVector &a, const CDoubleVector &b)
	{
		return CDoubleVector(a.x + b.x, a.y + b.y);
	}
	friend CDoubleVector operator--(const CDoubleVector &a)/*ȡʸ��a�ķ�����ʸ��*/
	{
		return CDoubleVector(-a.x, -a.y);
	}
	friend CDoubleVector operator-(const CDoubleVector &a, const CDoubleVector &b)
	{
		return CDoubleVector(a.x - b.x, a.y - b.y);
	}
	friend CDoubleVector& operator+=(CDoubleVector &a, const CDoubleVector &b)
	{
		a.x += b.x;
		a.y += b.y;
		return a;
	}
	friend CDoubleVector& operator-=(CDoubleVector &a, const CDoubleVector &b)
	{
		a.x -= b.x;
		a.y -= b.y;
		return a;
	}
	friend CDoubleVector operator*(const CDoubleVector &a, double f)
	{
		return CDoubleVector(a.x*f, a.y*f);
	}
	friend CDoubleVector operator*(double f, const CDoubleVector &a)
	{
		return CDoubleVector(a.x*f, a.y*f);
	}
	friend CDoubleVector operator/(const CDoubleVector &a, double f)
	{
		return CDoubleVector(a.x / f, a.y / f);
	}

	/*��������ʸ���ĳ˻�*/
	friend double operator^(const CDoubleVector &a, const CDoubleVector &b)
	{
		return a.x*b.x + a.y*b.y;
	}
};

struct CDoubleMatrix {
	double xx;
	double xy;
	double yx;
	double yy;
	CDoubleMatrix() :xx(0), xy(0), yx(0), yy(0)
	{}
	CDoubleMatrix(double axx, double axy, double ayx, double ayy) :
		xx(axx), xy(axy), yx(ayx), yy(ayy)
	{}

	friend CDoubleMatrix operator--(const CDoubleMatrix &a)/*ȡ����a����ԭ��ĶԳƵ�*/
	{
		return CDoubleMatrix(-a.xx, -a.xy, -a.yx, -a.yy);
	}
	friend CDoubleMatrix operator+(const CDoubleMatrix &a, const CDoubleMatrix &b)
	{
		return CDoubleMatrix(a.xx + b.xx, a.xy + b.xy, a.yx + b.yx, a.yy + b.yy);
	}
	friend CDoubleMatrix operator-(const CDoubleMatrix &a, const CDoubleMatrix &b)
	{
		return CDoubleMatrix(a.xx - b.xx, a.xy - b.xy, a.yx - b.yx, a.yy - b.yy);
	}

	friend CDoubleMatrix& operator+=(CDoubleMatrix &a, CDoubleMatrix &b)
	{
		a.xx += b.xx;
		a.xy += b.xy;
		a.yx += b.yx;
		a.yy += b.yy;
		return a;
	}
	friend CDoubleMatrix& operator-=(CDoubleMatrix &a, CDoubleMatrix &b)
	{
		a.xx -= b.xx;
		a.xy -= b.xy;
		a.yx -= b.yx;
		a.yy -= b.yy;
		return a;
	}

	friend CDoubleMatrix operator*(const CDoubleMatrix &a, double f)
	{
		return CDoubleMatrix(a.xx*f, a.xy*f, a.yx*f, a.yy*f);
	}
	friend CDoubleMatrix operator*(double f, const CDoubleMatrix &a)
	{
		return CDoubleMatrix(a.xx*f, a.xy*f, a.yx*f, a.yy*f);
	}
	friend CDoubleMatrix operator/(const CDoubleMatrix &a, double f)
	{
		return CDoubleMatrix(a.xx / f, a.xy / f, a.yx / f, a.yy / f);
	}
};

enum DoubleCellVariableID
{
	/*Cell Variable*/
	idMass,
	idPressure_cur,
	idPressure_half,
	idPressure_lag,
	idDensity_cur,
	idDensity_half,
	idDensity_lag,
	idInternalEnergy_cur,
	idInternalEnergy_half,
	idInternalEnergy_lag,
	idTotalEnergy_cur,
	idTotalEnergy_half,
	idTotalEnergy_lag,
	idSoundSpeed,
	idGamma,
	idVolume,
	idDivergence,
	idTotalWork,
	idKineticVariation,
	idCDensityGradient,
	idCPressureGradient,
	idCVorticity,
	 
	/*numbers of double cell variables*/
	idDoubleCellVariableNum,
};

enum IntEdgeVariableID
{
	idEdgeType, 

	idIntEdgeVariableNum,
};

enum IntCellVariableID
{
	idCoarseningTag, /*�����Ƿ����ֻ��ı�ǩ*/
	idAllowCoarsening,
	idAllowRefining,

	idIntCellVariableNum,
};








enum DoubleEdgeVariableID
{
	idERhoGradient,
	idEPressureGradient,

	idDoubleEdgeVariableNum,
};

enum VectorEdgeVariableID
{
	idEChildrenCoordinate_cur, /*edge middle variable to record the children information*/
	idEChidrenVelocity_cur,
	idEChildrenCoordinate_lag,
	idEChildrenVelocity_lag,
	idEChildrenCoordinate_bc,   /*bc means before coarsening*/
	idEChildrenVelocity_bc,

	idVectorEdgeVariableNum,
};

enum DoubleCornerVariableID
{
	/*Cell Corner Variable*/
	idReconstructPressure,
	idReconstructDensity,
	idVeloDeriToPoint,
	idCNRhoGradient,
	idCNPressGradient,
	idCNVorticity,

	/*numbers of double corner variables*/
	idDoubleCornerVariableNum,
};

enum VectorCellVariableID
{
	idCentroidCoord_cur,
	idCentroidCoord_half,
	idCentroidCoord_lag,
	idCentroidVelo_cur,
	idCentroidVelo_half,
	idCentroidVelo_lag,

	idRhoGradient,
	idPressGradient,
	idVeloXGradient,
	idVeloYGradient,

	idCentroidCoord_relaxed,

	/*numbers of vector cell variables*/
	idVectorCellVariableNum,
};

enum VectorCornerVariableID
{
	idcnCoords_cur,
	idcnCoords_half,
	idcnCoords_lag,
	idcnVelocity_cur,
	idcnVelocity_lag,
	idReconstructVelocity,
	idcnMcpUc,
	idcnRHS,
	ideMcpUc,
	ideRHS,
	idcnFcp,
	ideFcp,
	idAWFcp,
	idcnFluxRelaxed,
	idcnCoords_relaxed,
	idcnVelocity_relaxed,

	/*numbers of vector corner variables*/
	idVectorCornerVariableNum,
};

enum MatrixCornerVariableID
{
	idcnMcp,
	ideMcp,
	idcnAWMcp,

	idcnMatrixNum,
};

class CVariable
{
public:


/*four corners*/
/* 0 for left-bottom-corner,˳ʱ��*/



	double DouCData[idDoubleCellVariableNum];//double������������
	double DouCnData[idDoubleCornerVariableNum][CNDIM];//double�����������
	double DouEData[idDoubleEdgeVariableNum][CNDIM];//double�����������
	int    IntEData[idIntEdgeVariableNum][CNDIM];//int�����������
	int    IntCData[idIntCellVariableNum];//���͵�Ԫ��
	CDoubleVector  VecCData[idVectorCellVariableNum];//CDoubleVector������������
	CDoubleVector  VecCnData[idVectorCornerVariableNum][CNDIM];//CDoubleVector�����������
	CDoubleVector  VecEdata[idVectorEdgeVariableNum][CNDIM];
	CDoubleVector  ChildrenCnGeomVara[2/*0 for coord and 1 for velo*/][CNDIM/*four children*/][CNDIM/*four corner*/];
	double         ChildrenPhysicalVara[2/*0 for rho and 1 for internal energy*/][CNDIM/*four children*/];
	CDoubleMatrix  MarCnData[idcnMatrixNum][CNDIM];  //CDoubleMatrix�����������





public:
	CVariable();
	~CVariable();
	void CVariableRisize();
};