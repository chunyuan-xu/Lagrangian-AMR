#pragma once
#include "defines.h"
#include "core/vector_matrix.h"

// M14.4: pure hanging aggregation kernel.
namespace HydroCallbacks {

struct HangingAggregate {
	CDoubleMatrix matrix;
	CDoubleVector rhs;
};

inline HangingAggregate aggregate_hanging_matrix_rhs(
	const CDoubleMatrix &fine0_m, const CDoubleVector &fine0_rhs,
	const CDoubleMatrix &fine1_m, const CDoubleVector &fine1_rhs,
	const CDoubleMatrix &parent_m, const CDoubleVector &parent_rhs)
{
	HangingAggregate out;
	out.matrix = fine0_m + fine1_m + parent_m;
	out.rhs = fine0_rhs + fine1_rhs + parent_rhs;
	return out;
}

} // namespace HydroCallbacks
