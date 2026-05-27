#include "alg.h"
using namespace std;
#ifndef variable_const
#define M_PI 3.14159265358979323846
#endif // !variable_const


/*几何相关的函数*/
namespace GeometryAlg {
	double CalculateCellVolume(const int &coordtype, const CDoubleVector coord[4])
	{
		CDoubleVector center;
		double volume;
		center = GetPolyCenter(coord);
		//if (coordtype== p4est_data_t::MyCoordType::plane) {
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
	//计算叉积(p2-p1)*(p3-p2)
	double cross_product(const CDoubleVector &p1, const CDoubleVector &p2, const CDoubleVector &p3) {
		return (p2.x - p1.x)*(p3.y - p2.y) - (p2.y - p1.y)*(p3.x - p2.x);
	}

	
	//计算三角形重心 triangleCentroid







	//计算三角形面积（有向面积的绝对值）





	//判断四边形pts是否是凹四边形，如果是凸四边形，输出-1。
	//如果是凹四边形对应凹点分别为0，1，2，3时，分别输出0，1，2，3
	int is_concave_quad(const CDoubleVector pts[4])
	{
		CDoubleVector A = pts[0];
		CDoubleVector B = pts[1];
		CDoubleVector C = pts[2];
		CDoubleVector D = pts[3];
		const double eps = 1e-9;
		double cp1 = cross_product(A, B, C);//B
		double cp2 = cross_product(B, C, D);//C
		double cp3 = cross_product(C, D, A);//D
		double cp4 = cross_product(D, A, B);//A

		int sign1 = (cp1 > eps) ? 1 : (cp1 < -eps) ? -1 : 0;
		int sign2 = (cp2 > eps) ? 1 : (cp2 < -eps) ? -1 : 0;
		int sign3 = (cp3 > eps) ? 1 : (cp3 < -eps) ? -1 : 0;
		int sign4 = (cp4 > eps) ? 1 : (cp4 < -eps) ? -1 : 0;

		//检查是否全部非负（凸）
		if (sign1 >= 0 && sign2 >= 0 && sign3 >= 0 && sign4 >= 0) {
			return -1;//凸
		}

		//否则是凹四边形，输出凹点
		if (sign1 < 0) return 1;
		if (sign2 < 0) return 2;
		if (sign3 < 0) return 3;
		if (sign4 < 0) return 0;

	}

	//计算三角形质心







	//计算凹四边形的几何中心
	CDoubleVector concave_quad_centroid(const int & index, const CDoubleVector pts[4]) {
		//根据凹点位置选择分割方式
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

	/*double CalculateVectorDotVector(const CDoubleVector &va, const CDoubleVector &vb)
	{
		return va.x*vb.x + va.y*vb.y;
	}*/

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
			//printf("Error in GetLineNormalVector because line length is zero"); 

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

/*常用物理量计算*/
namespace PhysicalAlg {
	double CalculateCellMass(const double &volume, const double &density)
	{
		return volume*density;
	}

	double EquationOfState(const double &gamma, const double &density, const double &internal_energy)
	{
		return (gamma - 1.0)*density*internal_energy;
	};

	double CalculateSoundSpeed(const double &gamma, const double &pressure, const double &density)
	{
		if (sqrt(gamma*pressure / density) < m_eps)
		{
			printf("the sound speed is zero/n");
			abort();
		}
		return sqrt(gamma*pressure/ density);
	};

	double CalculateDivergence(const int &enumCoordType, const CDoubleVector coord[4], const CDoubleVector velocity[4])
	{
		double Divergence = 0.;
		double RcpPlus[4], LcpPlus[4], RcpMinus[4], LcpMinus[4];
		CDoubleVector NcpPlus[4], NcpMinus[4];
		int knext, kpre;
		for (int k = 0; k < 4; k++)
		{
			knext = GeometryAlg::GetCircleNext(4, k);
			kpre = GeometryAlg::GetCirclePre(4, k);
			if (enumCoordType == p4est_data_t::MyCoordType::plane)
			{
				RcpPlus[k] = 1.;
				RcpMinus[k] = 1.;
			}
			if (enumCoordType == p4est_data_t::MyCoordType::cylinder)
			{
				RcpPlus[k] = coord[k].y;
				RcpMinus[k] = coord[k].y;
			}
			LcpMinus[k] = 0.5*GeometryAlg::GetPointToPointDistance(coord[knext], coord[k]);
			LcpPlus[k] = 0.5*GeometryAlg::GetPointToPointDistance(coord[kpre], coord[k]);

			if (fabs(LcpMinus[k])>1e-12)
			{
				NcpMinus[k] = CDoubleVector(0.5*(coord[k].y - coord[knext].y) / LcpMinus[k],
					0.5*(coord[knext].x - coord[k].x) / LcpMinus[k]);
			}
			if (fabs(LcpPlus[k])>1e-12)
			{
				NcpPlus[k] = CDoubleVector(-0.5*(coord[k].y - coord[kpre].y) / LcpPlus[k],
					-0.5*(coord[kpre].x - coord[k].x) / LcpPlus[k]);
			}
		}

		for (int k = 0; k < 4; k++)
		{
			Divergence += RcpPlus[k] * LcpPlus[k] *
				(NcpPlus[k].x * velocity[k].x + NcpPlus[k].y * velocity[k].y) +
				RcpMinus[k] * LcpMinus[k] *
				(NcpMinus[k].x * velocity[k].x + NcpMinus[k].y * velocity[k].y);
			if (enumCoordType == p4est_data_t::MyCoordType::cylinder)
			{
				Divergence = 2. * M_PI *Divergence;
			}
		}
		return Divergence;
	};

	double get_VolumeVarationTimeStep(const double &Cv, const double &m_divergence)
	{
		double dt = 10000.;
		if (abs(m_divergence) > m_eps)
		{
			dt = Cv / abs(m_divergence);
		}
		return dt;
	}

	double get_CourantTimeStep(const CDoubleVector pts[CNDIM], const double &soundspeed)
	{
		double courant_num = 0.25;
		double dt = 10000000.;
		double edge_length[CNDIM];

		double min_len = 1e10;
		for (int i = 0; i < CNDIM; i++)
		{
			int inext = GeometryAlg::GetCircleNext(CNDIM, i);
			edge_length[i] = sqrt(pow(pts[i].x - pts[inext].x, 2) +
				pow(pts[i].y - pts[inext].y, 2));
			if (edge_length[i] < m_eps)
			{
				continue;
			}
			min_len = min(min_len, edge_length[i]);
		}
		if (soundspeed > 1e-12)
		{
			dt = min(dt, courant_num*min_len / soundspeed);
			if (dt < m_eps)
			{
				dt = 1000000.;
			}
		}
		else
		{
			printf("the sound speed is zero/n");
			abort();
		}
		return dt;
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
			if (CentroidCoordCur.x < 0.5 && CentroidCoordCur.y < 0.5) /*左下*/
			{
				CentroidVeloCur = CDoubleVector(0.8939, 0.8939);
				density_cur = 1.1;
				pressure_cur = 1.1;
			}
			if (CentroidCoordCur.x < 0.5 && CentroidCoordCur.y > 0.5) /*左上*/
			{
				CentroidVeloCur = CDoubleVector(0.8939, 0.);
				density_cur = 0.5065;
				pressure_cur = 0.35;
			}
			if (CentroidCoordCur.x > 0.5 && CentroidCoordCur.y < 0.5) /*右下*/
			{
				CentroidVeloCur = CDoubleVector(0., 0.8939);
				density_cur = 0.5065;
				pressure_cur = 0.35;
			}
			if (CentroidCoordCur.x > 0.5 && CentroidCoordCur.y > 0.5) /*右上*/
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
