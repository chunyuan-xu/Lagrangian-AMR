#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"

// M8.3: IOCallbacks — IO/Diagnostics quadrant callbacks stripped from main.cpp.

namespace IOCallbacks {


int convert_user_define_index_to_which_corner(const int &which_corner)
{
	int m_index;
	if (which_corner == 0) { m_index = 0; }
	if (which_corner == 3) { m_index = 1; }
	if (which_corner == 1) { m_index = 2; }
	if (which_corner == 2) { m_index = 3; }
	return m_index;
}

void quadrant_copy_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	vtu_cell_data_t	*m_cell_data = (vtu_cell_data_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset, corner_arrayoffset;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;

	arrayoffset = local_id;
	corner_arrayoffset = CNDIM * local_id;
	double		*p_val = (double *)sc_array_index(m_cell_data->pressure_array, arrayoffset);
	double		*t_val = (double *)sc_array_index(m_cell_data->temperature_array, arrayoffset);
	double		*rho_val = (double *)sc_array_index(m_cell_data->density_array, arrayoffset);
	double		*ie_val = (double *)sc_array_index(m_cell_data->internal_energy_array, arrayoffset);

	*p_val = m_vara->cell(idPressure_lag);
	*t_val = 0.0;
	*rho_val = m_vara->cell(idDensity_lag);
	*ie_val = m_vara->cell(idInternalEnergy_lag);
	for (int i = 0; i < CNDIM; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double *coordx_val = (double *)sc_array_index(m_cell_data->coordx, corner_arrayoffset + index0);
		coordx_val[0] = m_vara->corner_vector(idcnCoords_lag, i).x;

		double *coordy_val = (double *)sc_array_index(m_cell_data->coordy, corner_arrayoffset + index0);
		coordy_val[0] = m_vara->corner_vector(idcnCoords_lag, i).y;

		double *velox_val = (double *)sc_array_index(m_cell_data->velox, corner_arrayoffset + index0);
		velox_val[0] = m_vara->corner_vector(idcnVelocity_lag, i).x;

		double *veloy_val = (double *)sc_array_index(m_cell_data->veloy, corner_arrayoffset + index0);
		veloy_val[0] = m_vara->corner_vector(idcnVelocity_lag, i).y;
	}
}

void quadrant_write_distance_profiles_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;

	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_lag, i); }

	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	double distance;

	if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::radiusType)
	{
		distance = sqrt(pow(center_point.x, 2) + pow(center_point.y,2));
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::xType)
	{
		distance = fabs(center_point.x);
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::yType)
	{
		distance = fabs(center_point.y);
	}
	p4est_data->DistanceFile << blank << blank << distance <<
		blank << blank << m_vara->cell(idDensity_lag) <<
		blank << blank << m_vara->cell(idPressure_lag) <<
		blank << blank << m_vara->cell(idInternalEnergy_lag) <<
		blank << blank << m_vara->cell(idTotalEnergy_lag) << endl;
}

} // namespace IOCallbacks
