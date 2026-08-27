#pragma once
#include <cstdlib>
#include "defines.h"

// M11.2: typed RefineCriteria -> variable ID selector.
// Unsupported criteria abort before any variable-array access.
namespace AMRCallbacks {

struct RefinementVariableIds {
	DoubleCellVariableID source_cell;
	DoubleCellVariableID gradient_cell;
	DoubleEdgeVariableID edge;
	DoubleCornerVariableID corner;
};

inline RefinementVariableIds refinement_variable_ids(int refine_coarsen_enum)
{
	switch (refine_coarsen_enum) {
	case RefineCriteria::PressureGradient:
		return {idPressure_cur, idCPressureGradient,
			idEPressureGradient, idCNPressGradient};
	case RefineCriteria::DensityGradient:
		return {idDensity_cur, idCDensityGradient,
			idERhoGradient, idCNRhoGradient};
	default:
		std::abort();
	}
}

} // namespace AMRCallbacks
