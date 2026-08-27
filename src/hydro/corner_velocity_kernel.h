#pragma once
#include <cmath>
#include "defines.h"
#include "core/vector_matrix.h"
#include "alg.h"
#include "physics/corner_solve.h"

// M14.2: regular-corner velocity solve extracted from the callback.
namespace HydroCallbacks {

inline CDoubleVector solve_regular_corner_velocity(bool is_boundary,
	const CPointBounInfo &boun_plus, const CPointBounInfo &boun_minus,
	const CDoubleMatrix &matrix, const CDoubleVector &rhs)
{
	CDoubleVector velocity;
	if (is_boundary) {
		velocity = CornerSolve::boundary_node_velocity(
			boun_plus, boun_minus, matrix, rhs);
	} else {
		velocity = GeometryAlg::MatrixDotVector(
			GeometryAlg::MatrixInverse(matrix), rhs);
	}
	if (std::fabs(velocity.x) < m_eps) { velocity.x = 0.; }
	if (std::fabs(velocity.y) < m_eps) { velocity.y = 0.; }
	return velocity;
}

} // namespace HydroCallbacks
