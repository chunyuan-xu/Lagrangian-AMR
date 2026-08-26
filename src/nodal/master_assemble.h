#pragma once

#include "nodal/matrix_accessor.h"
#include "nodal/nodal_storage.h"

// S5b: assemble the enhanced master 2x2 matrix and RHS for one corner by
// summing the local direct block and any condensed hanging block.

namespace Nodal {

struct ShadowMaster {
	Matrix2 M;
	double b[2];
};

inline ShadowMaster assemble_shadow_master(
	const CellMasterContribution &local, std::uint8_t corner,
	const CondensedMasterContribution &condensed)
{
	ShadowMaster out;
	const std::uint8_t c = corner;
	out.M.m[0][0] = local.M[c][0] + condensed.M[c][0];
	out.M.m[0][1] = local.M[c][1] + condensed.M[c][1];
	out.M.m[1][0] = local.M[c][2] + condensed.M[c][2];
	out.M.m[1][1] = local.M[c][3] + condensed.M[c][3];
	out.b[0] = local.b[2 * c] + condensed.b[2 * c];
	out.b[1] = local.b[2 * c + 1] + condensed.b[2 * c + 1];
	return out;
}

} // namespace Nodal
