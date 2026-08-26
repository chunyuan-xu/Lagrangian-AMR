#pragma once

#include <cmath>
#include "defines.h"
#include "nodal/force_accessor.h"

namespace Nodal {

constexpr std::uint8_t kCornerSideA[kQuadCornerCount] = {0, 1, 0, 1};
constexpr std::uint8_t kCornerSideB[kQuadCornerCount] = {1, 0, 1, 0};

inline Vec2Storage legacy_corner_geometry_force(const quad_data_t &data, Corner corner)
{
	const std::uint8_t c = static_cast<std::uint8_t>(corner);
	Vec2Storage out = {0.0, 0.0};
	for (int i = 0; i < 2; ++i) {
		const std::uint8_t side = i == 0 ? kCornerSideA[c] : kCornerSideB[c];
		const CHalf_edge_data &h = data.m_cndata[c].hdata[side];
		out.x += h.Lcp * h.Ncp.x;
		out.y += h.Lcp * h.Ncp.y;
	}
	return out;
}

inline bool compare_vec(const Vec2Storage &a, const Vec2Storage &b, double tol = 1e-9)
{
	return std::fabs(a.x - b.x) <= tol && std::fabs(a.y - b.y) <= tol;
}

} // namespace Nodal
