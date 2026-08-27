#pragma once
#include <array>
#include <cmath>
#include <p4est.h>
#include "amr/coarsen_family_policy.h"
#include "amr/shock_front_policy.h"
#include "amr/refine_decision_policy.h"
#include "amr/coarsen_decision_policy.h"
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"

namespace AMRAgorithm {

inline double ShockFrontRadius(const p4est_data_t *p4est_data)
{
	if (p4est_data->which_case == ProblemNo::SedovCartesian ||
		p4est_data->which_case == ProblemNo::SedovPolar) {
		return ShockFrontPolicy::sedov_radius(
			p4est_data->current_time,
			p4est_data->distance_shock_radius_scale);
	}
	return ShockFrontPolicy::power_law_radius(
		p4est_data->current_time,
		p4est_data->distance_shock_radius_scale,
		p4est_data->distance_shock_radius_exponent);
}

inline ShockFrontPolicy::RadialBounds CellRadialBounds(
	const CVariable &vara, VectorCornerVariableID coordinate_id = idcnCoords_cur)
{
	std::array<std::array<double, 2>, CNDIM> corners;
	for (int corner = 0; corner < CNDIM; ++corner) {
		const CDoubleVector point = vara.corner_vector(coordinate_id, corner);
		corners[corner] = std::array<double, 2>{{point.x, point.y}};
	}
	return ShockFrontPolicy::radial_bounds(corners);
}

inline bool CellIntersectsShockBand(
	const CVariable &vara, double front_radius, double half_width)
{
	return ShockFrontPolicy::intersects_radial_band(
		CellRadialBounds(vara), front_radius, half_width);
}

inline int RefineErrorEstimate(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	quad_data_t  *data = (quad_data_t *)q->p.user_data;
	CVariable    *m_vara = (CVariable *)&data->m_vara;
	DoubleCellVariableID idCPara =
		AMRCallbacks::refine_gradient_indicator_id(
			p4est_data->refine_coarsen_enum);
	int           level = q->level;

	if (level < p4est_data->minus_level) {
		return 1;
	}
	if (level >= p4est_data->max_level) {
		return 0;
	}

	if (p4est_data->refine_coarsen_enum == RefineCriteria::Distance) {
		return CellIntersectsShockBand(
			*m_vara, ShockFrontRadius(p4est_data),
			p4est_data->distance_band_half_width) ? 1 : 0;
	}

	return m_vara->cell(idCPara) > p4est_data->refine_err ? 1 : 0;
}

inline int CoarsenErrorEstimate(
	p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *children[])
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	const DoubleCellVariableID idCPara =
		AMRCallbacks::coarsen_indicator_id(p4est_data->refine_coarsen_enum);
	const AMRCoarsenPolicy::IndicatorMode mode =
		AMRCallbacks::coarsen_indicator_mode(p4est_data->refine_coarsen_enum);

	if (mode == AMRCoarsenPolicy::IndicatorMode::DistanceFromShock) {
		const double front_radius = ShockFrontRadius(p4est_data);
		for (int i = 0; i < P4EST_CHILDREN; ++i) {
			quad_data_t *data = (quad_data_t *)children[i]->p.user_data;
			if (children[i]->level <= p4est_data->minus_level ||
				data->m_vara.int_cell(idAllowCoarsening) ==
					p4est_data_t::CoarseningEnum::CoarsingNotAllowed ||
				CellIntersectsShockBand(
					data->m_vara, front_radius,
					p4est_data->distance_band_half_width)) {
				return 0;
			}
		}
		return 1;
	}

	std::array<AMRCoarsenPolicy::ChildIndicator, P4EST_CHILDREN> family;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		quad_data_t *data = (quad_data_t *)children[i]->p.user_data;
		family[i] = AMRCoarsenPolicy::ChildIndicator{
			children[i]->level,
			data->m_vara.int_cell(idAllowCoarsening) !=
				p4est_data_t::CoarseningEnum::CoarsingNotAllowed,
			data->m_vara.cell(idCPara)
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

} // namespace AMRAgorithm
