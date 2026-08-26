#pragma once

#include "nodal/nodal_storage.h"

// S6a: recover the hanging velocity from the two master velocities.

namespace Nodal {

inline Vec2Storage recover_hanging_velocity(
	const Vec2Storage &master_a, const Vec2Storage &master_b,
	double weight_a = 0.5, double weight_b = 0.5)
{
	Vec2Storage out;
	out.x = weight_a * master_a.x + weight_b * master_b.x;
	out.y = weight_a * master_a.y + weight_b * master_b.y;
	return out;
}

} // namespace Nodal
