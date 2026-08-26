#pragma once

#include <cstdint>
#include "nodal/hanging_aggregate.h"
#include "nodal/nodal_storage.h"

// S4b: coarse owner accumulates endpoint.condensed += omega*aggregate for
// both endpoints.  Blocks 0/1 match the aggregate layout.

namespace Nodal {

inline bool apply_endpoint_condensation(CondensedMasterContribution &target,
	const AggregatedHangingContribution &agg, double omega, std::uint8_t endpoint)
{
	if (endpoint > 1 || omega < 0.0 || omega > 1.0) {
		return false;
	}
	const int row = endpoint;
	target.M[row][0] += omega * agg.M[row][0];
	target.M[row][1] += omega * agg.M[row][1];
	target.M[row][2] += omega * agg.M[row][2];
	target.M[row][3] += omega * agg.M[row][3];
	target.b[2 * endpoint] += omega * agg.b[2 * endpoint];
	target.b[2 * endpoint + 1] += omega * agg.b[2 * endpoint + 1];
	return true;
}

} // namespace Nodal
