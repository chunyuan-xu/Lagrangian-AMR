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

} // namespace AMRCallbacks
