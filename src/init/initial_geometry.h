#pragma once

#include "variable.h"

namespace InitialGeometry {

inline void seed_lag_corners(const CDoubleVector current[CNDIM],
	CDoubleVector lag[CNDIM])
{
	for (int i = 0; i < CNDIM; ++i) {
		lag[i] = current[i];
	}
}

} // namespace InitialGeometry
