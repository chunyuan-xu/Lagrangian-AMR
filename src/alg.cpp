#include "alg.h"
#include "physics/physics_alg.h"
using namespace std;
#ifndef variable_const
#define M_PI 3.14159265358979323846
#endif 


namespace GeometryAlg {
	double CalculateCellVolume(const int &coordtype, const CDoubleVector coord[4])
	{
		CDoubleVector center;
		double volume;
		center = GetPolyCenter(coord);
		
		CDoubleVector tri1[3], tri2[3];
			tri1[0] = coord[0];
			tri1[1] = coord[1];
			tri1[2] = coord[2];

			tri2[0] = coord[0];
			tri2[1] = coord[2];
			tri2[2] = coord[3];
			double area1 = CalculateTriangleArea(tri1);
			double area2 = CalculateTriangleArea(tri2);
			volume = area1 + area2;
		return volume;
	}

	double CalculateTriangleArea(const CDoubleVector coord[3]) {
		return fabs(coord[0].x*(coord[1].y - coord[2].y) 
			- coord[1].x*(coord[0].y - coord[2].y) +
			coord[2].x*(coord[0].y - coord[1].y)) / 2.0;
	}

	double GetPointToPointDistance2(const CDoubleVector &pt1, const CDoubleVector &pt2)
	{
		return square(pt1.x - pt2.x) + square(pt1.y - pt2.y);
	}

	double GetPointToPointDistance(const CDoubleVector &pt1, const CDoubleVector &pt2)
	{
		return _hypot(pt1.x - pt2.x, pt1.y - pt2.y);
	}
	
	double cross_product(const CDoubleVector &p1, const CDoubleVector &p2, const CDoubleVector &p3) {
		return (p2.x - p1.x)*(p3.y - p2.y) - (p2.y - p1.y)*(p3.x - p2.x);
	}

	
	int is_concave_quad(const CDoubleVector pts[4])
	{
		CDoubleVector A = pts[0];
		CDoubleVector B = pts[1];
		CDoubleVector C = pts[2];
		CDoubleVector D = pts[3];
		const double eps = 1e-9;
		double cp1 = cross_product(A, B, C);
		double cp2 = cross_product(B, C, D);
		double cp3 = cross_product(C, D, A);
		double cp4 = cross_product(D, A, B);

		int sign1 = (cp1 > eps) ? 1 : (cp1 < -eps) ? -1 : 0;
		int sign2 = (cp2 > eps) ? 1 : (cp2 < -eps) ? -1 : 0;
		int sign3 = (cp3 > eps) ? 1 : (cp3 < -eps) ? -1 : 0;
		int sign4 = (cp4 > eps) ? 1 : (cp4 < -eps) ? -1 : 0;

		
		if (sign1 >= 0 && sign2 >= 0 && sign3 >= 0 && sign4 >= 0) {
			return -1;
		}

		
		if (sign1 < 0) return 1;
		if (sign2 < 0) return 2;
		if (sign3 < 0) return 3;
		if (sign4 < 0) return 0;

	}

	
	CDoubleVector concave_quad_centroid(const int & index, const CDoubleVector pts[4]) {
		
		CDoubleVector centroid;
		CDoubleVector m_concave = pts[index];
		CDoubleVector m_1 = pts[(index + 2) % 4];
		centroid = 0.5 * (m_concave + m_1);


		return centroid;
	}

	CDoubleVector GetPolyCenter(const CDoubleVector coord[4]){
		CDoubleVector Centroid = CDoubleVector(0.0,0.0);
		double TotalArea = 0.0;
		CDoubleVector tcoord[3];
		double Area[2],
			x1, x2, y1, y2, TransformDet;
		for (int i = 0; i < 2; i++) {
			tcoord[0] = coord[0];
			tcoord[1] = coord[i + 1];
			tcoord[2] = coord[i + 2];
			Area[i] = CalculateTriangleArea(tcoord);
			TotalArea += Area[i];

			x1 = tcoord[1].x - tcoord[0].x;
			x2 = tcoord[2].x - tcoord[0].x;
			y1 = tcoord[1].y - tcoord[0].y;
			y2 = tcoord[2].y - tcoord[0].y;
			TransformDet = fabs(x1*y2-x2*y1);
			Centroid.x += TransformDet * (tcoord[0].x
				+ tcoord[1].x + tcoord[2].x) / 6.0;
			Centroid.y += TransformDet * (tcoord[0].y
				+ tcoord[1].y + tcoord[2].y) / 6.0;
		}
		if (fabs(TotalArea) > 1e-12) {
			Centroid = Centroid / TotalArea;
		}
		else{
			Centroid = CDoubleVector(0.0, 0.0);
			for (int i = 0; i < 4; i++) {
				Centroid = Centroid+coord[i];
			}
			Centroid = Centroid / 4.0;
		}
		return Centroid;
	}


	CDoubleVector GetPolyCenterByAverage(const CDoubleVector coord[4]) {
		CDoubleVector Centroid = CDoubleVector(0.0, 0.0);
		Centroid = CDoubleVector(0.0, 0.0);
		for (int i = 0; i < 4; i++) {
			Centroid = Centroid + coord[i];
		}
		Centroid = Centroid / 4.0;
		return Centroid;
	}

	int GetCircleNext(const int &num, const int &k) {
		int knext;
		knext = k + 1;
		if (num == knext) { knext = 0; }
		return knext;
	}

	int GetCirclePre(const int &num, const int &k) {
		int kpre;
		kpre = k - 1;
		if (kpre<0) { kpre = num - 1; }
		return kpre;
	}

	
	CDoubleMatrix DyadicProduct(const CDoubleVector va, const CDoubleVector vb)
	{
		CDoubleMatrix matrix;
		matrix.xx = va.x * vb.x;
		matrix.xy = va.x * vb.y;
		matrix.yx = va.y * vb.x;
		matrix.yy = va.y * vb.y;
		return matrix;
	}

	CDoubleMatrix MatrixInverse(const CDoubleMatrix ma)
	{
		CDoubleMatrix ma_inverse;
		double factor = ma.xx * ma.yy - ma.xy * ma.yx;
		ma_inverse.xx = ma.yy;
		ma_inverse.yy = ma.xx;
		ma_inverse.yx = -ma.yx;
		ma_inverse.xy = -ma.xy;
		ma_inverse = ma_inverse / factor;
		if (abs(factor) < m_eps)
		{
			return CDoubleMatrix(0.,0.,0.,0.);


		}
		return ma_inverse;
	}

	CDoubleVector MatrixDotVector(const CDoubleMatrix ma,
		const CDoubleVector va) 
	{
		CDoubleVector vb;
		vb.x = ma.xx*va.x + ma.xy*va.y;
		vb.y = ma.yx*va.x + ma.yy*va.y;
		return vb;
	}

	double GetRcpWeightWithLinearDistribution(const CDoubleVector &pts, const CDoubleVector &pte)
	{
		return (2.*pts.y + pte.y) / 3.;
	}

	double GetRcpWeightWithConstDistribution(const CDoubleVector &pts, const CDoubleVector &pte)
	{
		return (pts.y + pte.y) / 2.;
	}

	CDoubleVector GetLineNormalVector(const CDoubleVector &pts, const CDoubleVector &pte)
	{
		double length = sqrt(square(pts.x - pte.x) + square(pts.y - pte.y));
		if (length < 1e-15) 
		{ 
			return CDoubleVector(0., 0.);
			

		}
		return CDoubleVector(-(pte.y - pts.y), -(pts.x - pte.x)) / length;
	}

	CDoubleVector GetPointPointMiddle(const CDoubleVector &pts, const CDoubleVector &pte)
	{
		return 0.5 * (pts + pte);
	}

	double GetVectorValue(const CDoubleVector &va)
	{
		return sqrt(square(va.x) + square(va.y));
	}
}


namespace PhysicalAlg {
	
	
	double CalculateDivergence(const int &enumCoordType, const CDoubleVector coord[4], const CDoubleVector velocity[4])
{
	return PhysicalAlg::calculate_divergence(enumCoordType, coord, velocity);
}

double get_VolumeVarationTimeStep(const double &Cv, const double &m_divergence)
{
	return PhysicalAlg::volume_variation_time_step(Cv, m_divergence);
}

double get_CourantTimeStep(const CDoubleVector pts[CNDIM], const double &soundspeed)
{
	return PhysicalAlg::courant_time_step(pts, soundspeed);
}

void InitBoundaryCondition(const int &problem_index, const int &coord_type,
		int &TopBouType, int &BottomBouType, int &LeftBouType, int &RightBouType,
		double &TopBouVal, double &BottomBouVal, double &LeftBouVal, double &RightBouVal)
	{
		if (problem_index == ProblemNo::SedovCartesian)
		{
			TopBouType = FreeBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::SedovPolar)
		{
			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = CircleCenterBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::Sedov1DCartesian)
		{
			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::NohCartesian)
		{
			TopBouType = FreeBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::NohPolar)
		{
			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = CircleCenterBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::TriplePoint)
		{
			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		return;
	}

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
		double &TopBouVal, double &BottomBouVal, double &LeftBouVal, double &RightBouVal)
	{
		if (problem_index == ProblemNo::SedovCartesian)
		{
			double scale = 1.2;

			for (int i = 0; i < CNDIM; ++i){
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}

			density_cur = 1.0;
			density_lag = 1.0;

			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(0., 0.);
			CentroidVeloLag = CentroidVeloCur;

			if (qx == 0 && qy == 0)
			{
				internal_energy_cur = 0.244816 / mass;
			}
			else
			{
				internal_energy_cur = 1e-5;
			}
			internal_energy_lag = internal_energy_cur;
			gamma = 1.4;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = FreeBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::TwoDimRiemann)
		{
			double scale = 1.;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}
			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(0., 0.);
			gamma = 1.4;
			if (CentroidCoordCur.x < 0.5 && CentroidCoordCur.y < 0.5) 
			{
				CentroidVeloCur = CDoubleVector(0.8939, 0.8939);
				density_cur = 1.1;
				pressure_cur = 1.1;
			}
			if (CentroidCoordCur.x < 0.5 && CentroidCoordCur.y > 0.5) 
			{
				CentroidVeloCur = CDoubleVector(0.8939, 0.);
				density_cur = 0.5065;
				pressure_cur = 0.35;
			}
			if (CentroidCoordCur.x > 0.5 && CentroidCoordCur.y < 0.5) 
			{
				CentroidVeloCur = CDoubleVector(0., 0.8939);
				density_cur = 0.5065;
				pressure_cur = 0.35;
			}
			if (CentroidCoordCur.x > 0.5 && CentroidCoordCur.y > 0.5) 
			{
				CentroidVeloCur = CDoubleVector(0., 0.);
				density_cur = 1.1;
				pressure_cur = 1.1;
			}
			internal_energy_cur = pressure_cur / density_cur / (gamma - 1.);
			density_lag = density_cur;
			internal_energy_lag = internal_energy_cur;
			pressure_lag = pressure_cur;
			CentroidVeloLag = CentroidVeloCur;
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);
			
			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::SedovPolar)
		{
			double scale = 1.2;

			for (int i = 0; i < CNDIM; i++) {
				double m_value1 = M_PI / 2.*(index_j) / width_num;
				double m_value2 = M_PI / 2.*(index_j + 1) / width_num;
				double m_value3 = M_PI / 2. * (index_j + 1) / width_num;
				double m_value4 = M_PI / 2. * (index_j) / width_num;
				switch (i)
				{
				case 0:
					CoordCur[i].x = scale / width_num * index_i * cos(m_value1);
					CoordCur[i].y = scale / width_num * index_i * sin(m_value1);
					break;
				case 1:
					CoordCur[i].x = scale / width_num * index_i * cos(m_value2);
					CoordCur[i].y = scale / width_num * index_i * sin(m_value2);
					break;
				case 2:
					CoordCur[i].x = scale / width_num * (index_i+1) * cos(m_value3);
					CoordCur[i].y = scale / width_num * (index_i + 1) * sin(m_value3);
					break;
				case 3:
					CoordCur[i].x = scale / width_num * (index_i + 1) * cos(m_value4);
					CoordCur[i].y = scale / width_num * (index_i + 1) * sin(m_value4);
					break;
				}
				CoordLag[i] = CoordCur[i];
			}
			density_cur = 1.0;
			density_lag = 1.0;
			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(0., 0.);
			CentroidVeloLag = CentroidVeloCur;

			if (qx == 0)
			{
				internal_energy_cur = 0.244816 / mass / width_num;
			}
			else
			{
				internal_energy_cur = 1e-5;
			}
			internal_energy_lag = internal_energy_cur;
			gamma = 1.4;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = CircleCenterBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::Sedov1DCartesian)
		{
			double scale = 1.2;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}

			density_cur = 1.0;
			density_lag = 1.0;

			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(0., 0.);
			CentroidVeloLag = CentroidVeloCur;

			if (qx == 0)
			{
				internal_energy_cur = 0.244816 / mass;
			}
			else
			{
				internal_energy_cur = 1e-5;
			}
			internal_energy_lag = internal_energy_cur;
			gamma = 1.4;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::NohCartesian)
		{
			double scale = 1.;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
				double abs_radius = sqrt(pow(CoordCur[i].x, 2) + pow(CoordCur[i].y, 2));
				if (abs_radius > m_eps)
				{
					VeloCur[i] = CDoubleVector(-CoordCur[i].x / abs_radius,
						-CoordCur[i].y / abs_radius);
				}
				else
				{
					VeloCur[i] = CDoubleVector(0., 0.);
				}
				VeloLag[i] = VeloCur[i];
			}

			density_cur = 1.0;
			density_lag = 1.0;

			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			double center_radius = sqrt(pow(center_point.x, 2) + pow(center_point.y, 2));
			if (center_radius > m_eps)
			{
				CentroidVeloCur = CDoubleVector(-center_point.x / center_radius,-center_point.y / center_radius);
			}
			else
			{
				CentroidVeloCur = CDoubleVector(0., 0.);
			}
			CentroidVeloLag = CentroidVeloCur;

			internal_energy_cur = 1e-6;
			internal_energy_lag = internal_energy_cur;
			gamma = 5. / 3;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = FreeBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::NohPolar)
		{
			double scale = 1.;

			for (int i = 0; i < CNDIM; i++) {
				double m_value1 = M_PI / 2.*(index_j) / width_num;
				double m_value2 = M_PI / 2.*(index_j + 1) / width_num;
				double m_value3 = M_PI / 2. * (index_j + 1) / width_num;
				double m_value4 = M_PI / 2. * (index_j) / width_num;
				switch (i)
				{
				case 0:
					CoordCur[i].x = scale / width_num * index_i * cos(m_value1);
					CoordCur[i].y = scale / width_num * index_i * sin(m_value1);
					break;
				case 1:
					CoordCur[i].x = scale / width_num * index_i * cos(m_value2);
					CoordCur[i].y = scale / width_num * index_i * sin(m_value2);
					break;
				case 2:
					CoordCur[i].x = scale / width_num * (index_i + 1) * cos(m_value3);
					CoordCur[i].y = scale / width_num * (index_i + 1) * sin(m_value3);
					break;
				case 3:
					CoordCur[i].x = scale / width_num * (index_i + 1) * cos(m_value4);
					CoordCur[i].y = scale / width_num * (index_i + 1) * sin(m_value4);
					break;
				}
				CoordLag[i] = CoordCur[i];
				double abs_radius = sqrt(pow(CoordCur[i].x, 2) + pow(CoordCur[i].y, 2));
				if (abs_radius > m_eps)
				{
					VeloCur[i] = CDoubleVector(-CoordCur[i].x / abs_radius,
						-CoordCur[i].y / abs_radius);
				}
				else
				{
					VeloCur[i] = CDoubleVector(0., 0.);
				}
				VeloLag[i] = VeloCur[i];
			}

			density_cur = 1.0;
			density_lag = 1.0;

			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			CDoubleVector center_point;
			center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			double center_radius = sqrt(pow(center_point.x, 2) + pow(center_point.y, 2));
			if (center_radius > m_eps)
			{
				CentroidVeloCur = CDoubleVector(-center_point.x / center_radius, -center_point.y / center_radius);
			}
			else
			{
				CentroidVeloCur = CDoubleVector(0., 0.);
			}
			CentroidVeloLag = CentroidVeloCur;

			internal_energy_cur = 1e-6;
			internal_energy_lag = internal_energy_cur;
			gamma = 5. / 3;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = CircleCenterBoundary;
			RightBouType = FreeBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::TriplePoint)
		{
			double scale = 0.1;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}
			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			CDoubleVector center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(0., 0.);
			CentroidVeloLag = CentroidVeloCur;
			if (center_point.x < 1.)
			{
				density_cur = 1.0;
				density_lag = 1.0;
				internal_energy_cur = 2.5;
				gamma = 1.4;
			}
			else if (center_point.x > 1. && center_point.y < 1.5)
			{
				density_cur = 1.0;
				density_lag = 1.0;
				internal_energy_cur = 0.25;
				gamma = 1.4;
			}
			else if (center_point.x > 1. && center_point.y > 1.5)
			{
				density_cur = 0.125;
				density_lag = 0.125;
				internal_energy_cur = 2.;
				gamma = 1.5;
			}

			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			internal_energy_lag = internal_energy_cur;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::TaylorGreen)
		{
			double scale = 1.;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}
			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) {
				m_cell_coord[i] = CoordCur[i];
				VeloCur[i] = CDoubleVector(sin(M_PI*m_cell_coord[i].x)*cos(M_PI*m_cell_coord[i].y),
					-cos(M_PI*m_cell_coord[i].x)*sin(M_PI*m_cell_coord[i].y));
				VeloLag[i] = VeloCur[i];
			}
			CDoubleVector center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			CentroidVeloCur = CDoubleVector(sin(M_PI*CentroidCoordCur.x)*cos(M_PI*CentroidCoordCur.y),
				-cos(M_PI*CentroidCoordCur.x)*sin(M_PI*CentroidCoordCur.y));
			CentroidVeloLag = CentroidVeloCur;
			gamma = 1.4;
			density_cur = 1.0;
			density_lag = density_cur;
			pressure_cur = 0.25*density_cur*(cos(2.*M_PI * CentroidCoordCur.x) + cos(2.*M_PI*CentroidCoordCur.y)) + 1.;
			pressure_lag = pressure_cur;
			internal_energy_cur = pressure_cur / density_cur / (gamma - 1.);
			
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);
			internal_energy_lag = internal_energy_cur;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		if (problem_index == ProblemNo::SodCartesian)
		{
			double scale = 1.;

			for (int i = 0; i < CNDIM; ++i) {
				CoordCur[i] = scale * CoordCur[i];
				CoordLag[i] = CoordCur[i];
			}

			CDoubleVector m_cell_coord[CNDIM];
			for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = CoordCur[i]; }
			volume = GeometryAlg::CalculateCellVolume(coord_type, m_cell_coord);
			CDoubleVector center_point;
			center_point  = GeometryAlg::GetPolyCenter(m_cell_coord);
			CentroidCoordCur = center_point;
			double center_radius = sqrt(pow(center_point.x, 2) + pow(center_point.y, 2));

			if (center_radius <0.5)
			{
				density_cur = 1.0;
				internal_energy_cur = 1.5;
			}
			else
			{
				density_cur = 0.125;
				internal_energy_cur = 1.2;
			}

			density_lag = density_cur;
			CentroidVeloCur = CDoubleVector(0., 0.);
			CentroidVeloLag = CentroidVeloCur;
			
			mass = PhysicalAlg::CalculateCellMass(volume, density_cur);

			internal_energy_lag = internal_energy_cur;
			gamma = 5. / 3;
			pressure_cur = PhysicalAlg::EquationOfState(gamma, density_cur, internal_energy_cur);
			pressure_lag = pressure_cur;
			total_energy_cur = 0.5*(pow(CentroidVeloCur.x, 2) + pow(CentroidVeloCur.y, 2)) + internal_energy_cur;
			total_energy_lag = total_energy_cur;
			soundspeed = PhysicalAlg::CalculateSoundSpeed(gamma, pressure_cur, density_cur);

			TopBouType = WallBoundary;
			BottomBouType = WallBoundary;
			LeftBouType = WallBoundary;
			RightBouType = WallBoundary;

			TopBouVal = 0.;
			BottomBouVal = 0.;
			LeftBouVal = 0.;
			RightBouVal = 0.;
		}
		return;
	}
}
