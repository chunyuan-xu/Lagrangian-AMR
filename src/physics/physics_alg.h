#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "defines.h"
#include "core/vector_matrix.h"
#include "alg.h"

// M7.2: PhysicsAlg — pure physics helpers extracted from alg.cpp. These are
// stateless functions over scalar fields and 2D geometry; the cell-level
// orchestration (InitCondition/InitBoundaryCondition) stays in alg.cpp.

namespace PhysicalAlg {

inline double calculate_divergence(const int &enumCoordType,
	const CDoubleVector coord[4], const CDoubleVector velocity[4])
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
}

inline double volume_variation_time_step(const double &Cv, const double &m_divergence)
{
	double dt = 10000.;
	if (abs(m_divergence) > m_eps)
	{
		dt = Cv / abs(m_divergence);
	}
	return dt;
}

inline double courant_time_step(const CDoubleVector pts[CNDIM], const double &soundspeed)
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

} // namespace PhysicalAlg
