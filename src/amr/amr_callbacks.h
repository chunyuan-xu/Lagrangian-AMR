#pragma once
#include <algorithm>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "physics/physics_alg.h"
#include "physics/timestep_reduction.h"

// M8.1: AMRCallbacks — AMR-domain quadrant callbacks stripped from main.cpp.
// Each is a pure per-quadrant function over the p4est iterate context.

namespace AMRCallbacks {

void quadrant_predict_timestep_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	CDoubleVector corner_coords[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		corner_coords[cnid] = m_vara->corner_vector(idcnCoords_cur, cnid);
	}

	if (m_vara->cell(idSoundSpeed) < m_eps)
	{

	}
	
	
	double quad_cfl_dt = PhysicalAlg::get_CourantTimeStep(
		corner_coords, m_vara->cell(idSoundSpeed));

	
	double quad_vol_dt = PhysicalAlg::get_VolumeVarationTimeStep(
		p4est_data->volume_varation_torelarion,
		m_vara->cell(idDivergence));

	
	double quad_increased_dt = p4est_data->delta_time * p4est_data->dt_increase_percent;

	
	const double quad_dt = min(quad_cfl_dt,
		min(quad_vol_dt, quad_increased_dt));
	p4est_data->local_dt = TimestepReduction::accumulate_local_minimum(
		p4est_data->local_dt, quad_dt);

	if (p4est_data->local_dt < m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("Time step is too small in quad %d\n", info->quadid);
		abort();
	}
}

} // namespace AMRCallbacks
