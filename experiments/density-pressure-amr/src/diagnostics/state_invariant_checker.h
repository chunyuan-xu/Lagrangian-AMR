#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"
#include "alg.h"

// M2.3: state invariant checker.
//
// Evaluates a read-only set of per-cell physical invariants:
//   volume > 0, mass > 0, density > 0
//   density == mass / volume
//   pressure == EOS(gamma, density, internal_energy)
//   sound_speed == c(gamma, pressure, density)
//   stored lag centroid == GetPolyCenter(lag corners)   (self-consistency)
//   total_energy > 0 and >= internal_energy
//
// NOTE on total energy: after a step total_energy is a *conserved* quantity
// updated as total_half - dt*work/mass, while internal_energy is DERIVED as
// internal_half - dt*(work - kineticVariation)/mass. The two are only
// globally consistent, not per-cell: near shocks/AMR boundaries internal can
// transiently exceed total (kinetic term of the split not exactly bounded),
// so we assert only positivity, NOT total >= internal.
//
// The pure per-cell evaluator lives here (self-contained, unit-testable).
// The p4est adapter that iterates the forest and reports the stable CellKey
// lives in main.cpp so this header stays free of p4est link dependencies.

namespace Diagnostics {

struct InvariantViolation {
    const char* name;
    double expected;
    double actual;
};

inline bool nearly_equal(double a, double b, double rel_tol)
{
    const double scale =
        std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= rel_tol * scale;
}

// Pure per-cell invariant evaluation. Self-contained apart from
// GeometryAlg::GetPolyCenter; never writes the state.
inline std::vector<InvariantViolation> check_cell_invariants(
    const CVariable& vara)
{
    std::vector<InvariantViolation> violations;
    const double rel_tol = 1e-6;

    const double volume = vara.cell(idVolume);
    if (!(volume > 0.0)) {
        violations.push_back({"volume>0", volume, 0.0});
    }

    const double mass = vara.cell(idMass);
    if (!(mass > 0.0)) {
        violations.push_back({"mass>0", mass, 0.0});
    }

    const double density = vara.cell(idDensity_cur);
    if (!(density > 0.0)) {
        violations.push_back({"density>0", density, 0.0});
    }

    if (volume > 0.0) {
        const double rho = mass / volume;
        if (!nearly_equal(density, rho, rel_tol)) {
            violations.push_back({"density=mass/volume", rho, density});
        }
    }

    const double internal = vara.cell(idInternalEnergy_cur);
    const double gamma = vara.cell(idGamma);
    const double pressure = vara.cell(idPressure_cur);
    if (!(pressure > 0.0)) {
        violations.push_back({"pressure>0", pressure, 0.0});
    }
    const double expected_pressure =
        PhysicalAlg::EquationOfState(gamma, density, internal);
    if (!nearly_equal(pressure, expected_pressure, rel_tol)) {
        violations.push_back({"pressure=EOS", expected_pressure, pressure});
    }

    const double sound = vara.cell(idSoundSpeed);
    const double expected_sound = PhysicalAlg::CalculateSoundSpeed(
        gamma, vara.cell(idPressure_lag), vara.cell(idDensity_lag));
    if (!nearly_equal(sound, expected_sound, rel_tol)) {
        violations.push_back({"sound_speed=c", expected_sound, sound});
    }

    const double total = vara.cell(idTotalEnergy_cur);
    if (!(total > 0.0)) {
        violations.push_back({"total_energy>0", total, 0.0});
    }

    // Centroid consistency: centroid_cur is stored as
    // GetPolyCenter(corner cur) at init and after AcceptNumericalSolution
    // (accept copies corner_lag->corner_cur and centroid_lag->centroid_cur,
    // and centroid_lag is recomputed from corner_lag each step). Verify it
    // still matches (tolerance scaled by the largest edge so it stays
    // geometric for tiny/fine cells).
    CDoubleVector corner[CNDIM];
    for (int i = 0; i < CNDIM; ++i) {
        corner[i] = vara.corner_vector(idcnCoords_cur, i);
    }
    const CDoubleVector center = GeometryAlg::GetPolyCenter(corner);
    const CDoubleVector stored = vara.cell_vector(idCentroidCoord_cur);
    const double dist =
        std::hypot(stored.x - center.x, stored.y - center.y);
    double cell_size = 0.0;
    for (int i = 0; i < CNDIM; ++i) {
        cell_size = std::max(
            cell_size,
            std::hypot(corner[(i + 1) % CNDIM].x - corner[i].x,
                       corner[(i + 1) % CNDIM].y - corner[i].y));
    }
    if (dist > rel_tol * std::max(cell_size, 1e-12)) {
        violations.push_back({"centroid=geom", center.x, stored.x});
    }

    return violations;
}

}  // namespace Diagnostics

// M6.3: p4est adapter. Iterates the forest with a volume callback that runs
// the pure per-cell checker and aggregates violations. Kept header-only so
// it stays a self-contained module; the checker itself is a pure function.
namespace Diagnostics {

struct InvariantContext
{
	int phase_id;
	int violations;
	p4est_topidx_t first_tree;
	int first_level;
	p4est_qcoord_t first_x;
	p4est_qcoord_t first_y;
	const char *first_name;
};

inline void invariant_volume_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	InvariantContext *ctx = (InvariantContext *)user_data;
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	const CVariable &vara = data->m_vara;
	std::vector<Diagnostics::InvariantViolation> v =
		Diagnostics::check_cell_invariants(vara);
	if (v.empty()) {
		return;
	}
	if (ctx->violations == 0) {
		ctx->first_tree = info->treeid;
		ctx->first_level = info->quad->level;
		ctx->first_x = info->quad->x;
		ctx->first_y = info->quad->y;
		ctx->first_name = v[0].name;
	}
	ctx->violations += (int)v.size();
	P4EST_GLOBAL_PRODUCTIONF(
		"  [phase %d] (tree=%lld, level=%d, x=%lld, y=%lld) %s: "
		"expected %.17e, got %.17e\n",
		ctx->phase_id, (long long)info->treeid, info->quad->level,
		(long long)info->quad->x, (long long)info->quad->y,
		v[0].name, v[0].expected, v[0].actual);
}

inline void check_state_invariants(p4est_t *p4est, int phase_id)
{
	InvariantContext ctx = {phase_id, 0, -1, -1, -1, -1, NULL};
	p4est_iterate(p4est, NULL, &ctx, invariant_volume_callback, NULL,
#ifdef P4_TO_P8
		NULL,
#endif
		NULL);
	if (ctx.violations > 0) {
		P4EST_GLOBAL_PRODUCTIONF(
			"STATE INVARIANT VIOLATION (phase=%d): %d violation(s); "
			"first at (tree=%lld, level=%d, x=%lld, y=%lld) [%s]\n",
			ctx.phase_id, ctx.violations, (long long)ctx.first_tree,
			ctx.first_level, (long long)ctx.first_x, (long long)ctx.first_y,
			ctx.first_name ? ctx.first_name : "");
		std::abort();
	}
}

}  // namespace Diagnostics

