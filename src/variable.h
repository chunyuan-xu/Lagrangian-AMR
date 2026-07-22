#pragma once
#include <vector>
#include "core/vector_matrix.h"
#include "physics/eos.h"
#include <sc_options.h>
#include <algorithm>
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
#define fixed_iter_num 1  /*򵥵Ĭϵ*/
/*߽Ͷ*/
#define InnerBoundary     -1    //ڲ߽
#define PressureBoundary  1     //ѹ߽
#define WallBoundary      2     //̱ڱ߽
#define VelocityBoundary  3     //ٶȱ߽
#define FreeBoundary      4     //ɱ߽
#define SymmetryBoundary  5     //Գ߽
#define CircleCenterBoundary  6     //Ĳ߽

/*Ҷ*/
#define LeftIndex         0
#define RightIndex        1
#define BottomIndex       0
#define UpIndex           1

#define NotHanging     0
#define Hanging        1

#define m_eps           1e-12
#define m_coliner_eps   1e-13

#define blank '\\'
#endif // !p4est_ALE_const

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