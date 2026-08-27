#pragma once
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "defines.h"
#include "math.h"

namespace GeometryAlg {
	
	template<typename T>
	T square(T x)
	{
		return x*x;
	}

	template<typename T> int sgn(T t)
	{
		if (t > 0)
			return 1;
		else if (t < 0)
			return -1;
		else
			return 0;
	}

	
	double cross_product(const CDoubleVector &p1, const CDoubleVector & p2, const CDoubleVector &p3);

	
	int is_concave_quad(const CDoubleVector pts[4]);

	
	CDoubleVector concave_quad_centroid(const int & index, const CDoubleVector pts[4]);

	
	double CalculateCellVolume(const int &coordtype, const CDoubleVector coord[4]);

	
	CDoubleVector GetPolyCenter(const CDoubleVector coord[4]);

	
	CDoubleVector GetPolyCenterByAverage(const CDoubleVector coord[4]);

	
	int GetCircleNext(const int &num, const int &k);

	
	int GetCirclePre(const int &num, const int &k);

	
	double CalculateTriangleArea(const CDoubleVector coord[3]);

	
	double GetVectorValue(const CDoubleVector &va);

	
	CDoubleMatrix DyadicProduct(const CDoubleVector va, const CDoubleVector vb);

	
	CDoubleVector MatrixDotVector(const CDoubleMatrix ma,
		const CDoubleVector va);

	
	CDoubleMatrix MatrixInverse(const CDoubleMatrix ma);

	
	double GetPointToPointDistance2(const CDoubleVector &pt1, const CDoubleVector &pt2);

	
	double GetPointToPointDistance(const CDoubleVector &pt1, const CDoubleVector &pt2);

	// M11.5: pure zero-distance guard for gradient geometry. Rejects zero
	// distance instead of clamping it.
	inline double guarded_point_distance(const CDoubleVector &pt1,
		const CDoubleVector &pt2, const char *label)
	{
		const double dist = GetPointToPointDistance(pt1, pt2);
		if (!(dist > m_eps)) {
			fprintf(stderr, "Zero distance in %s\n",
				label ? label : "guarded_point_distance");
			std::abort();
		}
		return dist;
	}

	
	double GetRcpWeightWithLinearDistribution(const CDoubleVector &pts, const CDoubleVector &pte);

	
	double GetRcpWeightWithConstDistribution(const CDoubleVector &pts, const CDoubleVector &pte);

	
	CDoubleVector GetLineNormalVector(const CDoubleVector &pts, const CDoubleVector &pte);

	
	CDoubleVector GetPointPointMiddle(const CDoubleVector &pts, const CDoubleVector &pte);
}

namespace PhysicalAlg {
	
	double CalculateCellMass(const double &volume, const double &density);

	double get_CourantTimeStep(const CDoubleVector pts[CNDIM], const double &soundspeed);

	double get_VolumeVarationTimeStep(const double &Cv, const double &m_divegence);

	
	double EquationOfState(const double &gamma, const double &density, const double &internal_energy);

	
	double CalculateSoundSpeed(const double &gamma, const double &pressure, const double &density);

	
	double CalculateDivergence(const int &enumCoordType, const CDoubleVector coord[4], const CDoubleVector velocity[4]);

	void InitBoundaryCondition(const int &problem_index, const int &coord_type,
		int &TopBouType, int &BottomBouType, int &LeftBouType, int &RightBouType,
		double &TopBouVal, double &BottomBouVal, double &LeftBouVal, double &RightBouVal);

	void InitCondition(const int &problem_index, const int &coord_type,
		int qx, int qy, const int &index_i, const int &index_j, const int &width_num,
		CDoubleVector CoordCur[CNDIM], CDoubleVector CoordLag[CNDIM],
		CDoubleVector VeloCur[CNDIM], CDoubleVector VeloLag[CNDIM],
		double &density_cur, double &density_lag,
		double &volume, double &mass,
		CDoubleVector &CentroidCoordCur, CDoubleVector &CentroidCoordLag,
		CDoubleVector &CentroidVeloCur, CDoubleVector &CentroidVeloLag,
		double &internal_energy_cur, double &internal_energy_lag,
		double &pressure_cur, double &pressure_lag,
		double &total_energy_cur, double &total_energy_lag,
		double &soundspeed, double &gamma,
		int &TopBouType, int &BottomBouType, int &LeftBouType, int &RightBouType,
		double &TopBouVal, double &BottomBouVal, double &LeftBouVal, double &RightBouVal);
}


