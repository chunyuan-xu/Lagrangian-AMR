#pragma once
#include "defines.h"

// M10L.2: repair for the confirmed read-before-write path
// points[].pi_constrained_parent -> ParentBounInfo::ParentPIStar.
// The field had no production writer; before quadrant_set_init_parent_edge_callback
// reads it, this helper defines every corner slot to zero. ParentPIStar is
// currently dormant, so this does not change numerical behavior.
namespace AMRCallbacks {

inline void reset_point_pi_constrained_parent(quad_data_t &data)
{
	for (int cnid = 0; cnid < CNDIM; ++cnid) {
		data.points[cnid].pi_constrained_parent = 0.;
	}
}

// M10L.4: transient-state reset for one ParentBounInfo slot. This helper is
// intentionally not wired into any event yet; it only defines the reset
// contract for FluxRelaxed so inactive reads are well-defined.
inline void reset_parent_edge_scratch(ParentBounInfo &info)
{
	info.FluxRelaxed = CDoubleVector(0., 0.);
}

} // namespace AMRCallbacks
