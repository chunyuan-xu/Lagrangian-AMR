#pragma once
#include "defines.h"
#include "variable.h"
#include "core/vector_matrix.h"
#include "alg.h"

// M14.7: pure volume/density update kernel.
namespace HydroCallbacks {

inline void update_volume_density(CVariable &vara, int coord_type)
{
	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; ++i) {
		m_cell_coord[i] = vara.corner_vector(idcnCoords_lag, i);
	}
	vara.cell(idVolume) = GeometryAlg::CalculateCellVolume(
		coord_type, m_cell_coord);
	vara.cell(idDensity_lag) = vara.cell(idMass) / vara.cell(idVolume);
}

} // namespace HydroCallbacks
