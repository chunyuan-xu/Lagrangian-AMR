#pragma once

#include <cmath>
#include "nodal/master_assemble.h"
#include "nodal/nodal_storage.h"

// S5c: solve one local 2x2 shadow master system.

namespace Nodal {

struct Solve2x2Result {
	bool ok;
	Vec2Storage velocity;
	const char *reason;
};

inline Solve2x2Result solve_shadow_master(const ShadowMaster &sys,
	double det_tol = 1e-12)
{
	const double a00 = sys.M.m[0][0];
	const double a01 = sys.M.m[0][1];
	const double a10 = sys.M.m[1][0];
	const double a11 = sys.M.m[1][1];
	if (!std::isfinite(a00) || !std::isfinite(a01) || !std::isfinite(a10) || !std::isfinite(a11) ||
		!std::isfinite(sys.b[0]) || !std::isfinite(sys.b[1])) {
		return Solve2x2Result{false, {0.0, 0.0}, "nonfinite master system"};
	}
	const double det = a00 * a11 - a01 * a10;
	if (std::fabs(det) < det_tol) {
		return Solve2x2Result{false, {0.0, 0.0}, "singular or near-singular master"};
	}
	Solve2x2Result out;
	out.ok = true;
	out.reason = nullptr;
	out.velocity.x = (a11 * sys.b[0] - a01 * sys.b[1]) / det;
	out.velocity.y = (a00 * sys.b[1] - a10 * sys.b[0]) / det;
	return out;
}

} // namespace Nodal
