#pragma once

#include <cmath>
#include "nodal/nodal_storage.h"

// S7b: shadow-only candidate next-corner coordinates and volume.

namespace Nodal {

struct CandidateGeometry {
	double coords[4][2];
	double volume;
};

inline CandidateGeometry build_candidate_geometry(
	const double coords[4][2], const Vec2Storage velocities[4], double dt)
{
	CandidateGeometry out;
	for (int c = 0; c < 4; ++c) {
		out.coords[c][0] = coords[c][0] + velocities[c].x * dt;
		out.coords[c][1] = coords[c][1] + velocities[c].y * dt;
	}
	double vol = 0.0;
	for (int c = 0; c < 4; ++c) {
		const int n = (c + 1) % 4;
		vol += out.coords[c][0] * out.coords[n][1] -
			out.coords[n][0] * out.coords[c][1];
	}
	out.volume = 0.5 * std::fabs(vol);
	return out;
}

} // namespace Nodal
