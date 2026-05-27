#pragma once
#include <iostream>
#include "defines.h"
#include "math.h"

namespace GeometryAlg {
	/*平方, square*/
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

	//计算叉积(p2-p1)*(p3-p2)
	double cross_product(const CDoubleVector &p1, const CDoubleVector & p2, const CDoubleVector &p3);

	//计算三角形重心
	//

	//计算三角形面积（有向面积的绝对值）
	//

	//计算三角形质心
	//

	//判断四边形pts是否是凹四边形，如果是凸四边形，输出-1
	//如果是凹四边形，对应凹点分别为0，1，2，3时，分别输出0，1，2，3
	int is_concave_quad(const CDoubleVector pts[4]);

	//计算凹四边形的几何中心
	CDoubleVector concave_quad_centroid(const int & index, const CDoubleVector pts[4]);

	//点是否在多边形内部
	//
	//

	/*计算网格的体积*/
	double CalculateCellVolume(const int &coordtype, const CDoubleVector coord[4]);

	/*计算四边形的几何中心*/
	CDoubleVector GetPolyCenter(const CDoubleVector coord[4]);

	/*几何平均法求解四边形中心*/
	CDoubleVector GetPolyCenterByAverage(const CDoubleVector coord[4]);

	/*计算k+1*/
	int GetCircleNext(const int &num, const int &k);

	/*计算k-1*/
	int GetCirclePre(const int &num, const int &k);

	/*计算三角形的面积*/
	double CalculateTriangleArea(const CDoubleVector coord[3]);

	/*计算矢量的幅值*/
	double GetVectorValue(const CDoubleVector &va);

	///*计算矢量va和vb的乘积*/
	//double CalculateVectorDotVector(const CDoubleVector &va, const CDoubleVector &vb);

	/*计算矢量va和vb的并矢*/
	CDoubleMatrix DyadicProduct(const CDoubleVector va, const CDoubleVector vb);

	/*计算矩阵ma和矢量va的乘积*/
	CDoubleVector MatrixDotVector(const CDoubleMatrix ma,
		const CDoubleVector va);

	/*计算矩阵的逆*/
	CDoubleMatrix MatrixInverse(const CDoubleMatrix ma);

	/*计算两点之间距离的平方*/
	double GetPointToPointDistance2(const CDoubleVector &pt1, const CDoubleVector &pt2);

	/*计算两点之间的距离*/
	double GetPointToPointDistance(const CDoubleVector &pt1, const CDoubleVector &pt2);

	/*计算柱坐标系下边的权值Rcp，注意速度沿该边呈线性分布*/
	double GetRcpWeightWithLinearDistribution(const CDoubleVector &pts, const CDoubleVector &pte);

	/*计算柱坐标系下边的权值Rcp，注意速度沿该边呈常数分布*/
	double GetRcpWeightWithConstDistribution(const CDoubleVector &pts, const CDoubleVector &pte);

	/*线段pts-pte逆时针旋转90度后的向量*/
	CDoubleVector GetLineNormalVector(const CDoubleVector &pts, const CDoubleVector &pte);

	/*计算pts-pte线段的中点ptm*/
	CDoubleVector GetPointPointMiddle(const CDoubleVector &pts, const CDoubleVector &pte);
}

namespace PhysicalAlg {
	/*计算网格的质量*/
	double CalculateCellMass(const double &volume, const double &density);

	double get_CourantTimeStep(const CDoubleVector pts[CNDIM], const double &soundspeed);

	double get_VolumeVarationTimeStep(const double &Cv, const double &m_divegence);

	/*通过状态方程计算网格的压力*/
	double EquationOfState(const double &gamma, const double &density, const double &internal_energy);

	/*计算网格的声速*/
	double CalculateSoundSpeed(const double &gamma, const double &pressure, const double &density);

	/*计算单元散度*/
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




