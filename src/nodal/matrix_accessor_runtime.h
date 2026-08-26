#pragma once

#include <cmath>
#include "defines.h"
#include "nodal/matrix_accessor.h"

namespace Nodal {

inline constexpr std::uint8_t kCornerSideA[kQuadCornerCount] = {0, 1, 0, 1};
inline constexpr std::uint8_t kCornerSideB[kQuadCornerCount] = {1, 0, 1, 0};

inline Matrix2 legacy_corner_geometry_matrix(const quad_data_t &data, Corner corner)
{
	const std::uint8_t c = static_cast<std::uint8_t>(corner);
	Matrix2 out = { { { 0.0, 0.0 }, { 0.0, 0.0 } } };
	for (int i = 0; i < 2; ++i) {
		const std::uint8_t side = i == 0 ? kCornerSideA[c] : kCornerSideB[c];
		const CHalf_edge_data &h = data.m_cndata[c].hdata[side];
		Matrix2 term = dyadic_product(h.Ncp.x, h.Ncp.y);
		term.m[0][0] *= h.Lcp;
		term.m[0][1] *= h.Lcp;
		term.m[1][0] *= h.Lcp;
		term.m[1][1] *= h.Lcp;
		out.m[0][0] += term.m[0][0];
		out.m[0][1] += term.m[0][1];
		out.m[1][0] += term.m[1][0];
		out.m[1][1] += term.m[1][1];
	}
	return out;
}

inline bool compare_matrix(const Matrix2 &a, const Matrix2 &b, double tol = 1e-9)
{
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			if (std::fabs(a.m[i][j] - b.m[i][j]) > tol) {
				return false;
			}
		}
	}
	return true;
}

} // namespace Nodal
