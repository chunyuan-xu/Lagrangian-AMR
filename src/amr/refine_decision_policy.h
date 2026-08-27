#pragma once
#include "defines.h"
#include "variable.h"
#include "alg.h"

// M13.4: refine decision policy helpers.
namespace AMRCallbacks {

inline DoubleCellVariableID refine_gradient_indicator_id(int refine_coarsen_enum)
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

inline int default_refine_tag_value(const CVariable &vara)
{
	CDoubleVector m_coord[CNDIM];
	for (int cnid = 0; cnid < CNDIM; ++cnid) {
		m_coord[cnid] =
			vara.corner_vector(idcnCoords_lag, CNDIM - 1 - cnid);
	}
	return GeometryAlg::is_concave_quad(m_coord);
}

} // namespace AMRCallbacks
