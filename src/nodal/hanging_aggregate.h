#pragma once

#include <cstring>
#include "nodal/matrix_accessor.h"
#include "nodal/nodal_storage.h"

// S3b: aggregate two fine-cell local hanging contributions into the coarse
// owner's AggregatedHangingContribution.  Block 0 corresponds to master a,
// block 1 to master b, each a 2x2 M and 2-vector b.

namespace Nodal {

struct HangingAggregateInput {
	Matrix2 M_a;
	Matrix2 M_b;
	double b_a[2];
	double b_b[2];
};

inline void aggregate_hanging_contribution(
	const HangingAggregateInput &in, AggregatedHangingContribution &out)
{
	std::memset(&out, 0, sizeof(out));
	out.M[0][0] = in.M_a.m[0][0];
	out.M[0][1] = in.M_a.m[0][1];
	out.M[0][2] = in.M_a.m[1][0];
	out.M[0][3] = in.M_a.m[1][1];
	out.M[1][0] = in.M_b.m[0][0];
	out.M[1][1] = in.M_b.m[0][1];
	out.M[1][2] = in.M_b.m[1][0];
	out.M[1][3] = in.M_b.m[1][1];
	out.b[0] = in.b_a[0];
	out.b[1] = in.b_a[1];
	out.b[2] = in.b_b[0];
	out.b[3] = in.b_b[1];
}

} // namespace Nodal
