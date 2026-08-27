#pragma once
#include <cmath>
#include "defines.h"
#include "alg.h"

// M13.1: pure conforming two-cell gradient kernel.
namespace AMRCallbacks {

inline double conforming_gradient(double para_a, double para_b,
	const CDoubleVector &center_a, const CDoubleVector &center_b)
{
	const double dist = GeometryAlg::guarded_point_distance(
		center_a, center_b, "AMR conforming gradient");
	return std::fabs(para_a - para_b) / dist;
}

// M13.2: pure coarse/fine hanging gradient between one parent-like value and
// one fine child value.
inline double hanging_gradient(double parent_para, double child_para,
	const CDoubleVector &parent_center, const CDoubleVector &child_center)
{
	const double dist = GeometryAlg::guarded_point_distance(
		parent_center, child_center, "AMR hanging gradient");
	return std::fabs(parent_para - child_para) / dist;
}

} // namespace AMRCallbacks
