#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <p4est.h>
#include "amr/coarsen_family_policy.h"
#include "amr/shock_front_policy.h"
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"
#include "amr/pressure_experiment.h"
#include "amr/pressure_sided_experiment.h"
#include "amr/pressure_pair_math.h"
#include "amr/compression_experiment.h"
#include "amr/directional_length_experiment.h"
#include "amr/coarse_priority_experiment.h"

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

inline ShockFrontPolicy::RadialBounds CellRadialBoundsFromPoint(
	const CVariable &vara, double center_x, double center_y,
	VectorCornerVariableID coordinate_id = idcnCoords_cur)
{
	std::array<std::array<double, 2>, CNDIM> corners;
	for (int corner = 0; corner < CNDIM; ++corner) {
		const CDoubleVector point = vara.corner_vector(coordinate_id, corner);
		corners[corner][0] = point.x - center_x;
		corners[corner][1] = point.y - center_y;
	}
	return ShockFrontPolicy::radial_bounds(corners);
}

// Build an annular topology before the first hydro update.  A cell is
// refined when its physical radial interval intersects the requested band.
// Runtime AMR and repartition are disabled while initial_annulus_mesh is set.
inline int InitialAnnulusRefineErrorEstimate(
	p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{
	(void) which_tree;
	p4est_data_t *p4est_data =
		&((P4estBridge *)p4est->user_pointer)->data;
	if (!p4est_data->initial_annulus_mesh ||
		q->level >= p4est_data->initial_annulus_level) {
		return 0;
	}

	const quad_data_t *data =
		static_cast<const quad_data_t *>(q->p.user_data);
	const ShockFrontPolicy::RadialBounds bounds = CellRadialBoundsFromPoint(
		data->m_vara,
		p4est_data->initial_annulus_center_x,
		p4est_data->initial_annulus_center_y);
	return bounds.maximum >= p4est_data->initial_annulus_inner_radius &&
		bounds.minimum <= p4est_data->initial_annulus_outer_radius ? 1 : 0;
}

// Equivalent-square side length of the current physical quadrilateral.
// idVolume stores the two-dimensional physical cell area and is refreshed
// after Lagrangian mesh motion and AMR transfer.
inline double CellCharacteristicLength(const CVariable &vara)
{
	const double area = vara.cell(idVolume);
	if (!std::isfinite(area) || area <= 0.0) {
		return std::numeric_limits<double>::quiet_NaN();
	}
	return std::sqrt(area);
}

// Dimensionless density-gradient sensor:
//     eta_rho = h * |grad(rho)| / max(|rho|, rho_floor).
// The gradient magnitude comes from the configured density estimator. Returning
// infinity for invalid cell state forces refinement (when possible) and
// prevents unsafe coarsening.
inline double DimensionlessDensityGradientIndicator(const CVariable &vara)
{
	double h = CellCharacteristicLength(vara);
	if (DirectionalLengthExperiment::enabled()) {
		const int order[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};
		std::array<std::array<double,2>,4> points;
		for(int i=0;i<4;++i){const auto p=vara.corner_vector(idcnCoords_cur,order[i]);points[i]={{p.x,p.y}};}
		const auto g=vara.cell_vector(idAMRDensityGradient);
		h=DirectionalLengthExperiment::length(points,vara.cell(idVolume),g.x,g.y);
	}
	const double density = std::abs(vara.cell(idDensity_cur));
	const double gradient = std::abs(vara.cell(idCDensityGradient));
	if (!std::isfinite(h) || !std::isfinite(density) ||
		!std::isfinite(gradient)) {
		return std::numeric_limits<double>::infinity();
	}
	return h * gradient / std::max(density, static_cast<double>(m_eps));
}

inline int RefineByDimensionlessDensityGradient(
	const p4est_data_t &p4est_data, const p4est_quadrant_t &quadrant,
	const CVariable &vara)
{
	if (quadrant.level < p4est_data.minus_level) {
		return 1;
	}
	if (quadrant.level >= p4est_data.max_level) {
		return 0;
	}
    const double eta=DimensionlessDensityGradientIndicator(vara);
    if(CompressionExperiment::runtime_enabled() && CompressionExperiment::indicator(vara)>CompressionExperiment::threshold()) return 1;
    const double selected_pressure=PressureSidedExperiment::select(
        vara.cell(idAMRPressureJump),vara.cell(idAMRPressureSidedJump),true,
        quadrant.level,p4est_data.max_level,PressureSidedExperiment::mode());
    if(!std::isfinite(eta) || !std::isfinite(selected_pressure)) return 1;
    // Isolated experiment: leave lower-level resolution untouched. Restrict
    // only entry into the finest level to significant-pressure cells.
    if(quadrant.level==p4est_data.max_level-1 && PressureExperiment::finest_fraction()>0. &&
       std::abs(vara.cell(idPressure_cur))<=PressureExperiment::finest_fraction()*PressureExperiment::maximum_pressure()) return 0;
    bool selected=PressureExperiment::keep(eta>p4est_data.refine_err,selected_pressure,PressureExperiment::refine());
    if(PressurePairExperiment::active(true,quadrant.level,p4est_data.max_level,PressurePairExperiment::mode())) {
        const double flag=vara.cell(idAMRPressurePairRefine);
        if(!std::isfinite(flag))return 1;
        selected=flag>0.;
    }
    selected=selected && !CoarsePriorityExperiment::veto_refine(quadrant);
    return selected ? 1 : 0;
}

inline int CoarsenByDimensionlessDensityGradient(
	const p4est_data_t &p4est_data, p4est_quadrant_t *children[])
{
    if(PressureExperiment::trace() && children[0]->level==6 && children[0]->x==637534208 && children[0]->y==603979776 && p4est_data.current_time>.59) {
        for(int c=0;c<4;++c) {const auto &v=static_cast<const quad_data_t *>(children[c]->p.user_data)->m_vara;std::printf("PTRACE COARSEN step=%d child=%d eta=%.17g jp=%.17g tag=%d\n",p4est_data.current_step,c,DimensionlessDensityGradientIndicator(v),v.cell(idAMRPressureJump),v.int_cell(idAllowCoarsening));}
    }
	bool above_maximum_level = false;
	for (int i = 0; i < P4EST_CHILDREN; ++i) {
		const quad_data_t *data =
			static_cast<const quad_data_t *>(children[i]->p.user_data);
		if (children[i]->level <= p4est_data.minus_level ||
			data->m_vara.int_cell(idAllowCoarsening) ==
				p4est_data_t::CoarseningEnum::CoarsingNotAllowed) {
			return 0;
		}
		above_maximum_level = above_maximum_level ||
			children[i]->level > p4est_data.max_level;
		// Invalid state must veto even the maximum-level override below.
		if (!std::isfinite(
			DimensionlessDensityGradientIndicator(data->m_vara))) {
			return 0;
		}
	}

	if (above_maximum_level) {
		return 1;
	}

	if (CoarsePriorityExperiment::prefer_coarsen(children)) return 1;

	// A p4est family is coarsened as one unit. Requiring every child to be
	// smooth prevents one low-indicator child from hiding a shock in a sibling.
	for (int i = 0; i < P4EST_CHILDREN; ++i) {
		const quad_data_t *data =
			static_cast<const quad_data_t *>(children[i]->p.user_data);
		const double indicator =
			DimensionlessDensityGradientIndicator(data->m_vara);
        if(CompressionExperiment::runtime_enabled() && CompressionExperiment::indicator(data->m_vara)>=.5*CompressionExperiment::threshold()) return 0;
        const double selected_pressure=PressureSidedExperiment::select(
            data->m_vara.cell(idAMRPressureJump),data->m_vara.cell(idAMRPressureSidedJump),false,
            children[i]->level,p4est_data.max_level,PressureSidedExperiment::mode());
        bool retain=PressureExperiment::keep(indicator>=p4est_data.coarsen_error,selected_pressure,PressureExperiment::coarsen());
        if(PressurePairExperiment::active(false,children[i]->level,p4est_data.max_level,PressurePairExperiment::mode())) {
            const double flag=data->m_vara.cell(idAMRPressurePairRetain);
            if(!std::isfinite(flag))return 0;
            retain=flag>0.;
        }
        if (!std::isfinite(indicator) || !std::isfinite(selected_pressure) ||
            (retain &&
             !(children[i]->level==p4est_data.max_level && PressureExperiment::finest_fraction()>0. &&
               std::abs(data->m_vara.cell(idPressure_cur))<=.5*PressureExperiment::finest_fraction()*PressureExperiment::maximum_pressure()))) {
			return 0;
		}
	}
	return 1;
}

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
		return CellIntersectsShockBand(
			*m_vara, ShockFrontRadius(p4est_data),
			p4est_data->distance_band_half_width) ? 1 : 0;
	}
	if (p4est_data->refine_coarsen_enum ==
		RefineCriteria::DimensionlessDensityGradient) {
		return RefineByDimensionlessDensityGradient(
			*p4est_data, *q, *m_vara);
	}

	return m_vara->cell(idCPara) > p4est_data->refine_err ? 1 : 0;
}

inline int CoarsenErrorEstimate(
	p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *children[])
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	if (p4est_data->refine_coarsen_enum ==
		RefineCriteria::DimensionlessDensityGradient) {
		return CoarsenByDimensionlessDensityGradient(
			*p4est_data, children);
	}

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
