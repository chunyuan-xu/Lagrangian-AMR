#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "core/trace.h"

// M8.3: IOCallbacks — IO/Diagnostics quadrant callbacks stripped from main.cpp.

namespace IOCallbacks {

// M9.3.3: global field checksum probe (Kahan summation + MPI reduce).
void StatGlobalFieldChecksum(p4est_t *p4est, const char* label) {
    double local_sums[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double c[5] = {0.0, 0.0, 0.0, 0.0, 0.0};

    p4est_tree_t *tree;
    p4est_quadrant_t *quad;
    sc_array_t *tquadrants;
    for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; ++t) {
        tree = p4est_tree_array_index (p4est->trees, t);
        tquadrants = &tree->quadrants;
        for (size_t i = 0; i < tquadrants->elem_count; ++i) {
            quad = p4est_quadrant_array_index (tquadrants, i);
            quad_data_t *data = (quad_data_t *)quad->p.user_data;

            double vals[5] = {
                data->m_vara.cell(idMass),
                data->m_vara.cell(idTotalEnergy_lag),
                data->m_vara.cell(idDensity_lag),
                data->m_vara.cell_vector(idCentroidVelo_lag).x + data->m_vara.cell_vector(idCentroidVelo_lag).y,
                data->m_vara.cell(idTotalWork)
            };

            for(int k=0; k<5; ++k) {
                double y = vals[k] - c[k];
                double t_val = local_sums[k] + y;
                c[k] = (t_val - local_sums[k]) - y;
                local_sums[k] = t_val;
            }
        }
    }

    double global_sums[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    sc_MPI_Reduce(local_sums, global_sums, 5, sc_MPI_DOUBLE, sc_MPI_SUM, 0, p4est->mpicomm);

    if (p4est->mpirank == 0) {
        P4EST_GLOBAL_PRODUCTIONF("Checksum [%s]: Mass = %.14e, E = %.14e, Rho = %.14e, V = %.14e, W = %.14e\n",
            label, global_sums[0], global_sums[1], global_sums[2], global_sums[3], global_sums[4]);
    }
}


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


void quadrant_total_energy_error_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	p4est_data->total_energy_lag += m_vara->cell(idMass) * m_vara->cell(idTotalEnergy_lag);
	p4est_data->total_energy_cur += m_vara->cell(idMass) * m_vara->cell(idTotalEnergy_cur);


}
} // namespace IOCallbacks
