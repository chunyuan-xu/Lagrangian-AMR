#pragma once
#include <p4est.h>

// M6.1: MeshAdapter — stable cell identity helpers. A cell is identified by
// its global SFC index, computed from the rank's first global quadrant plus
// its local offset. This is a pure function so business layers never need to
// cast raw void* pointers to recover identity.

namespace MeshAdapter {

inline double global_sfc_id(p4est_t *p4est, p4est_locidx_t local_id)
{
	return (double)(p4est->global_first_quadrant[p4est->mpirank] + local_id);
}

} // namespace MeshAdapter
