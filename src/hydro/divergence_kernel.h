#pragma once
#include "defines.h"
#include "core/vector_matrix.h"
#include "physics/physics_alg.h"

// M14.5: pure divergence kernel.
namespace HydroCallbacks {

inline double compute_divergence(int coord_type,
	const CDoubleVector coord[CNDIM], const CDoubleVector velocity[CNDIM])
{
	return PhysicalAlg::CalculateDivergence(coord_type, coord, velocity);
}

} // namespace HydroCallbacks
