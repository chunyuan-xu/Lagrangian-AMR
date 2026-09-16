#pragma once
#include <p4est.h>

// M7.5: HydroPhases — pure hydro update stage wrappers extracted from
// main.cpp. Each stage runs a caller-supplied volume callback over the
// forest with the p4est_data_t context, exactly as the legacy wrappers did.
// This keeps the module boundary while leaving the per-cell callback bodies
// (which read CVariable fields) in the adapter translation unit.

namespace HydroPhases {

typedef void (*volume_cb)(p4est_iter_volume_info_t *, void *);

inline void run_volume_update(p4est_t *p4est, volume_cb cb)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		cb,
		NULL,
#ifdef P4_TO_P8
		NULL,
#endif
		NULL);
}

} // namespace HydroPhases
