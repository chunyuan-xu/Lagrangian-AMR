#pragma once
#include <cmath>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"

namespace AMRAgorithm {

// Estimates whether a quadrant should be refined based on user criteria
inline int RefineErrorEstimate(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t  *data = (quad_data_t *)q->p.user_data;
	CVariable    *m_vara = (CVariable *)&data->m_vara;
	int           idCPara = idCDensityGradient;
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
		idCPara = idCentroidCoord_cur;
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
		double dist = std::sqrt(std::pow(m_vara->VecCData[idCentroidCoord_cur].x, 2) +
			                    std::pow(m_vara->VecCData[idCentroidCoord_cur].y, 2));
		if (std::fabs(dist - p4est_data->shock_velocity * p4est_data->current_time) < p4est_data->refine_err) {
			return 1;
		} else {
			return 0;
		}
	}

	if (m_vara->DouCData[idCPara] > p4est_data->refine_err) {
		return 1;
	} else {
		return 0;
	}
}

// Estimates whether a family of 4 quadrants should be coarsened based on user criteria
inline int CoarsenErrorEstimate(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *children[])
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t  *data;
	int           idCPara = idCDensityGradient;
	double        parent_gradient = 0.;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	for (int i = 0; i < P4EST_CHILDREN; i++) {
		data = (quad_data_t *)children[i]->p.user_data;
		int level = children[i]->level;

		if (data->m_vara.IntCData[idAllowCoarsening] == p4est_data_t::CoarseningEnum::CoarsingNotAllowed) {
			return 0;
		}
		if (level <= p4est_data->minus_level) {
			return 0;
		}
		if (level > p4est_data->max_level) {
			return 1;
		}

		if (p4est_data->refine_coarsen_enum == RefineCriteria::Distance) {
			double dist = std::sqrt(std::pow(data->m_vara.VecCData[idCentroidCoord_cur].x, 2) +
				                    std::pow(data->m_vara.VecCData[idCentroidCoord_cur].y, 2));
			if (std::fabs(dist - p4est_data->shock_velocity * p4est_data->current_time) > p4est_data->coarsen_error) {
				return 1;
			} else {
				return 0;
			}
		}

		if (data->m_vara.DouCData[idCPara] < p4est_data->coarsen_error) {
			return 1;
		}
		parent_gradient += data->m_vara.DouCData[idCPara];
	}

	parent_gradient /= P4EST_CHILDREN;
	if (parent_gradient < p4est_data->coarsen_error) {
		return 1;
	} else {
		return 0;
	}
}

} // namespace AMRAgorithm
