#pragma once
#include "defines.h"
#include "variable.h"
#include "amr/coarsen_family_policy.h"

// M13.5: coarsen decision policy helpers.
namespace AMRCallbacks {

inline DoubleCellVariableID coarsen_indicator_id(int refine_coarsen_enum)
{
	switch (refine_coarsen_enum) {
	case RefineCriteria::PressureGradient:
		return idCPressureGradient;
	case RefineCriteria::DensityGradient:
		return idCDensityGradient;
	default:
		return idCDensityGradient;
	}
}

inline AMRCoarsenPolicy::IndicatorMode coarsen_indicator_mode(
	int refine_coarsen_enum)
{
	if (refine_coarsen_enum == RefineCriteria::Distance) {
		return AMRCoarsenPolicy::IndicatorMode::DistanceFromShock;
	}
	return AMRCoarsenPolicy::IndicatorMode::Gradient;
}

} // namespace AMRCallbacks
