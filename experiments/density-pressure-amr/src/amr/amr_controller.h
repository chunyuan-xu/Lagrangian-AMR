#pragma once
#include <p4est.h>
#include <p4est_extended.h>
#include "mesh/ghost_session.h"
#include "amr/pressure_experiment.h"
#include "amr/coarsen_plan_experiment.h"
#include "diagnostics/roundtrip_audit.h"
#include "amr/same_pass_experiment.h"
#include "diagnostics/refresh_geometry_audit.h"
#include "amr/overlap_remap_experiment.h"

// M4.4: AMRController — extracts the AMR stage orchestration from the main
// time loop. Stage order and p4est parameters are identical to the original
// inline block: refine -> rebuild ghost -> coarsen tag -> coarsen -> balance
// -> destroy ghost. Diagnostic logging is kept in the caller.

namespace AMRController {

typedef int (*refine_fn)(p4est_t *, p4est_topidx_t, p4est_quadrant_t *);
typedef int (*coarsen_fn)(p4est_t *, p4est_topidx_t, p4est_quadrant_t **);
typedef void (*replace_fn)(p4est_t *, p4est_topidx_t, int,
	p4est_quadrant_t **, int, p4est_quadrant_t **);
typedef void (*tag_fn)(p4est_t *, GhostSession &);
typedef void (*energy_fn)(p4est_t *);

inline void execute_amr(p4est_t *p4est, GhostSession &session,
	int recursive, int allowed_level, int callbackorphans,
	refine_fn refine_cb, coarsen_fn coarsen_cb, replace_fn replace_cb,
	tag_fn tag_cb, energy_fn energy_cb)
{
    SamePassExperiment::Pass same_pass;
    RefreshGeometryAudit::volume(p4est,"before_amr");
    CoarsePriorityExperiment::prepare(p4est,AMRAgorithm::DimensionlessDensityGradientIndicator);
    if(SamePassExperiment::mode()) {
        SC_CHECK_ABORT(recursive==0 && callbackorphans==0,"Same-pass experiment requires nonrecursive families");
        SamePassExperiment::prepare(p4est,coarsen_cb,same_pass);
        coarsen_cb=SamePassExperiment::coarsen;
    }
    PressureExperiment::stage()="refine";
	p4est_refine_ext(p4est, recursive, allowed_level,
		refine_cb, NULL, replace_cb);
    RefreshGeometryAudit::volume(p4est,"after_refine");
    OverlapRemapExperiment::capture(p4est);

	session.invalidate_after_topology_change();
	session.rebuild(p4est, P4EST_CONNECT_FULL);

	tag_cb(p4est, session);

    CoarsenPlanExperiment::Plan plan;
    RoundtripAudit::Snapshot roundtrip_before;
    if(CoarsenPlanExperiment::mode()) {
        SC_CHECK_ABORT(recursive==0 && callbackorphans==0,"Preview supports nonrecursive family coarsening only");
        CoarsenPlanExperiment::prepare(p4est,coarsen_cb,plan);
        roundtrip_before=RoundtripAudit::capture(plan);
        CoarsenPlanExperiment::active()=&plan;
    }

    PressureExperiment::stage()="coarsen";
	p4est_coarsen_ext(p4est, recursive, callbackorphans,
		CoarsenPlanExperiment::mode()==2 ? CoarsenPlanExperiment::guarded_coarsen : coarsen_cb, NULL, replace_cb);
    RefreshGeometryAudit::volume(p4est,"after_coarsen");
    OverlapRemapExperiment::after_coarsen(p4est);
    if(CoarsenPlanExperiment::mode()) {
        const auto &run=static_cast<P4estBridge*>(p4est->user_pointer)->data;
        P4EST_GLOBAL_PRODUCTIONF("COARSEN_PLAN step=%d t=%.17g mode=%d requested=%d undone=%d veto=%d\n",run.current_step,run.current_time,CoarsenPlanExperiment::mode(),(int)plan.requested.size(),(int)plan.blocked.size(),plan.applied_veto);
        CoarsenPlanExperiment::active()=nullptr;
    }

	energy_cb(p4est);
    PressureExperiment::stage()="balance";
	p4est_balance_ext(p4est, P4EST_CONNECT_CORNER, NULL,
		replace_cb);
    RefreshGeometryAudit::volume(p4est,"after_balance");
    if(CoarsenPlanExperiment::mode()) {
        CoarsenPlanExperiment::verify_topology(p4est,plan);
        RoundtripAudit::compare(p4est,roundtrip_before);
    }

	session.invalidate_after_topology_change();
	session.destroy();
    SamePassExperiment::active()=nullptr;
    CoarsePriorityExperiment::finish(p4est);
}

inline void execute_partition(p4est_t *p4est, GhostSession &session,
	int allowcoarsening)
{
	p4est_partition(p4est, allowcoarsening, NULL);
	session.invalidate_after_topology_change();
	session.destroy();
}

} // namespace AMRController
