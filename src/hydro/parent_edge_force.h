#pragma once

#include "defines.h"

namespace ParentEdgeForce {

inline CDoubleVector evaluate(
	const ParentBounInfo &info,
	const CDoubleVector &reconstructed_velocity,
	double reconstructed_pressure,
	const CDoubleMatrix &matrix)
{
	if (!info.IsParentChildBoun) {
		return CDoubleVector(0.0, 0.0);
	}

	const CDoubleVector delta_velocity =
		info.Hanging_velocity - reconstructed_velocity;
	const CDoubleVector weighted_normal =
		info.Lcp[0] * info.Ncp[0] + info.Lcp[1] * info.Ncp[1];
	const CDoubleVector matrix_delta(
		matrix.xx * delta_velocity.x + matrix.xy * delta_velocity.y,
		matrix.yx * delta_velocity.x + matrix.yy * delta_velocity.y);
	return weighted_normal * reconstructed_pressure - matrix_delta;
}

} // namespace ParentEdgeForce
