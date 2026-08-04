#pragma once
#include <vector>
#include "core/vector_matrix.h"
#include "physics/eos.h"
#include <sc_options.h>
#include <algorithm>
using namespace std;

#ifndef p4est_ALE_const
#define CNDIM   4
#define P4EST_DIM 2  
#define fixed_iter_num 1  

#define InnerBoundary     -1    
#define PressureBoundary  1     
#define WallBoundary      2     
#define VelocityBoundary  3     
#define FreeBoundary      4     
#define SymmetryBoundary  5     
#define CircleCenterBoundary  6     


#define fixed_iter_num 1  

#define InnerBoundary     -1    
#define PressureBoundary  1     
#define WallBoundary      2     
#define VelocityBoundary  3     
#define FreeBoundary      4     
#define SymmetryBoundary  5     
#define CircleCenterBoundary  6     


#define LeftIndex         0
#define RightIndex        1
#define BottomIndex       0
#define UpIndex           1

#define NotHanging     0
#define Hanging        1

#define m_eps           1e-12
#define m_coliner_eps   1e-13

#define blank '\\'
#endif 

enum DoubleCellVariableID
{
	
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
	 
	
	idDoubleCellVariableNum,
};

enum IntCellVariableID
{
	idCoarseningTag, 
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
	idEChildrenCoordinate_cur, 
	idEChidrenVelocity_cur,
	idEChildrenCoordinate_lag,
	idEChildrenVelocity_lag,
	idEChildrenCoordinate_bc,   
	idEChildrenVelocity_bc,

	idVectorEdgeVariableNum,
};

enum DoubleCornerVariableID
{
	
	idReconstructPressure,
	idReconstructDensity,
	idVeloDeriToPoint,
	idCNRhoGradient,
	idCNPressGradient,
	idCNVorticity,

	
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


	double DouCData[idDoubleCellVariableNum];
	double DouCnData[idDoubleCornerVariableNum][CNDIM];
	double DouEData[idDoubleEdgeVariableNum][CNDIM];
	int    IntCData[idIntCellVariableNum];
	CDoubleVector  VecCData[idVectorCellVariableNum];
	CDoubleVector  VecCnData[idVectorCornerVariableNum][CNDIM];
	CDoubleVector  VecEdata[idVectorEdgeVariableNum][CNDIM];
	CDoubleVector  ChildrenCnGeomVara[2][CNDIM][CNDIM];
	double         ChildrenPhysicalVara[2][CNDIM];
	CDoubleMatrix  MarCnData[idcnMatrixNum][CNDIM];  


public:
	inline double &cell(DoubleCellVariableID id) noexcept
	{
		return DouCData[id];
	}

	inline const double &cell(DoubleCellVariableID id) const noexcept
	{
		return DouCData[id];
	}

	inline double &corner(DoubleCornerVariableID id, int corner_id) noexcept
	{
		return DouCnData[id][corner_id];
	}

	inline const double &corner(
		DoubleCornerVariableID id, int corner_id) const noexcept
	{
		return DouCnData[id][corner_id];
	}

	inline double &edge(DoubleEdgeVariableID id, int edge_id) noexcept
	{
		return DouEData[id][edge_id];
	}

	inline const double &edge(
		DoubleEdgeVariableID id, int edge_id) const noexcept
	{
		return DouEData[id][edge_id];
	}

	inline CDoubleVector &corner_vector(
		VectorCornerVariableID id, int corner_id) noexcept
	{
		return VecCnData[id][corner_id];
	}

	inline const CDoubleVector &corner_vector(
		VectorCornerVariableID id, int corner_id) const noexcept
	{
		return VecCnData[id][corner_id];
	}

	inline CDoubleVector &cell_vector(VectorCellVariableID id) noexcept
	{
		return VecCData[id];
	}

	inline const CDoubleVector &cell_vector(
		VectorCellVariableID id) const noexcept
	{
		return VecCData[id];
	}

	inline CDoubleVector &edge_vector(
		VectorEdgeVariableID id, int edge_id) noexcept
	{
		return VecEdata[id][edge_id];
	}

	inline const CDoubleVector &edge_vector(
		VectorEdgeVariableID id, int edge_id) const noexcept
	{
		return VecEdata[id][edge_id];
	}

	inline int &int_cell(IntCellVariableID id) noexcept
	{
		return IntCData[id];
	}

	inline const int &int_cell(IntCellVariableID id) const noexcept
	{
		return IntCData[id];
	}

	CVariable();
	~CVariable();
	void CVariableRisize();
};