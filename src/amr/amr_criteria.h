#pragma once
#include <array>
#include <cmath>
#include <p4est.h>
#include "amr/coarsen_family_policy.h"
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"

namespace AMRAgorithm {


inline int RefineErrorEstimate(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	quad_data_t  *data = (quad_data_t *)q->p.user_data;
	CVariable    *m_vara = (CVariable *)&data->m_vara;
	DoubleCellVariableID idCPara = idCDensityGradient;
	int           level = q->level;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		// idCentroidCoord_cur is a VectorCellVariableID, incompatible with the
		// DoubleCellVariableID idCPara. The assignment was dead code: the
		// Distance path returns early below before cell(idCPara) is read.
		break;
	default:
		break;
	}

	if (level < p4est_data->minus_level) {
		return 1;
	}
	if (level >= p4est_data->max_level) {
		return 0;
	}

	if (p4est_data->refine_coarsen_enum == RefineCriteria::Distance) {
		double dist = std::sqrt(std::pow(m_vara->cell_vector(idCentroidCoord_cur).x, 2) +
			                    std::pow(m_vara->cell_vector(idCentroidCoord_cur).y, 2));
		if (std::fabs(dist - p4est_data->shock_velocity * p4est_data->current_time) < p4est_data->refine_err) {
			return 1;
		} else {
			return 0;
		}
	}

	if (m_vara->cell(idCPara) > p4est_data->refine_err) {
		return 1;
	} else {
		return 0;
	}
}


inline int CoarsenErrorEstimate(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *children[])
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	DoubleCellVariableID idCPara = idCDensityGradient;
	AMRCoarsenPolicy::IndicatorMode mode = AMRCoarsenPolicy::IndicatorMode::Gradient;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		mode = AMRCoarsenPolicy::IndicatorMode::DistanceFromShock;
		break;
	default:
		break;
	}

	std::array<AMRCoarsenPolicy::ChildIndicator, P4EST_CHILDREN> family;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		quad_data_t *data = (quad_data_t *)children[i]->p.user_data;
		double indicator = data->m_vara.cell(idCPara);
		if (mode == AMRCoarsenPolicy::IndicatorMode::DistanceFromShock) {
			const double dist = std::sqrt(
				std::pow(data->m_vara.cell_vector(idCentroidCoord_cur).x, 2) +
				std::pow(data->m_vara.cell_vector(idCentroidCoord_cur).y, 2));
			indicator = std::fabs(
				dist - p4est_data->shock_velocity * p4est_data->current_time);
		}
		family[i] = AMRCoarsenPolicy::ChildIndicator{
			children[i]->level,
			data->m_vara.int_cell(idAllowCoarsening) !=
				p4est_data_t::CoarseningEnum::CoarsingNotAllowed,
			indicator
		};
	}

	const AMRCoarsenPolicy::FamilyPolicy policy{
		mode,
		p4est_data->minus_level,
		p4est_data->max_level,
		p4est_data->coarsen_error
	};
	return AMRCoarsenPolicy::family_allows_coarsening(family, policy) ? 1 : 0;
}

} 
