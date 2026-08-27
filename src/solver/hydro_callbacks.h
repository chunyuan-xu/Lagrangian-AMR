#pragma once
#include <cmath>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "amr/parent_edge_view.h"
#include "hydro/divergence_kernel.h"
#include "hydro/energy_kernel.h"
#include "hydro/momentum_kernel.h"
#include "hydro/volume_density_kernel.h"
#include "hydro/work_kernel.h"
#include "physics/eos.h"
#include "physics/physics_alg.h"

// M7.5: HydroPhases — per-cell hydro update callbacks. These are the concrete
// numerical field-update formulas (density, momentum, work, energy, EOS,
// sound speed, divergence). They are pure per-cell functions over the
// quadrant data; the forest traversal wrapper lives in HydroPhases::run_volume_update.

namespace HydroPhases {


void quadrant_compute_divergence_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	
	
	CDoubleVector	cnVelocity[CNDIM];
	CDoubleVector	cnCoord[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		cnCoord[k] = m_vara->corner_vector(idcnCoords_lag, k);
		cnVelocity[k] = m_vara->corner_vector(idcnVelocity_lag, k);
	}
	m_vara->cell(idDivergence) = HydroCallbacks::compute_divergence(
		p4est_data->coord_type, cnCoord, cnVelocity);
}

void quadrant_compute_soundspeed_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
		m_vara->cell(idGamma),
		m_vara->cell(idPressure_lag),
		m_vara->cell(idDensity_lag));
}

void quadrant_update_density_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	HydroCallbacks::update_volume_density(*m_vara, coordinate_type);
}

void quadrant_update_momentum_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	int					scheme_type = p4est_data->Scheme_type;
	AMRCallbacks::ParentEdgeView	parent_edges(*data);
	HydroCallbacks::update_momentum(
		*m_vara, parent_edges, coordinate_type, scheme_type,
		p4est_data->dt_iter);
}

void quadrant_compute_work_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	HydroCallbacks::update_work(*m_vara, PCInfo, coordinate_type);
}

void quadrant_update_energy_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	HydroCallbacks::update_energy(
		*m_vara, p4est_data->dt_iter,
		static_cast<int>(p4est_data->which_case), info->quadid);
}

void quadrant_update_EOS_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idPressure_lag) = PhysicalAlg::EquationOfState(m_vara->cell(idGamma), m_vara->cell(idDensity_lag), m_vara->cell(idInternalEnergy_lag));
	if (m_vara->cell(idPressure_lag) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the value of pressure is illegal\n");
	}
}

} // namespace HydroPhases
