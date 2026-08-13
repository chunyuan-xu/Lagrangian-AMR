#pragma once
#include <fstream>
#include <p4est.h>
#include <p4est_vtk.h>
#include "defines.h"
#include "variable.h"
#include "core/trace.h"
#include "io/output_stamp.h"
#include "mesh/cell_key.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

// M8.3: IOCallbacks — IO/Diagnostics quadrant callbacks stripped from main.cpp.

namespace IOCallbacks {

// M10.1.1: file-handle management. The ofstream handles previously lived
// inside p4est_data_t (a non-POD god object). Opening them lazily here,
// only on first write, keeps quad_data_t / p4est_data_t POD-free of
// streams and avoids file handles being copied/destroyed in MPI contexts.
inline std::ofstream &energy_error_file()
{
	static std::ofstream f("EnergyError.plt");
	if (!f.is_open()) {
		f.open("EnergyError.plt");
	}
	f.setf(std::ios::fixed, std::ios::floatfield);
	f.precision(16);
	return f;
}

inline std::ofstream &distance_profile_file()
{
	static std::ofstream f("DistanceProfiles.plt");
	if (!f.is_open()) {
		f.open("DistanceProfiles.plt");
	}
	f.setf(std::ios::fixed, std::ios::floatfield);
	f.precision(16);
	return f;
}

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
	std::ofstream &distance_file = distance_profile_file();
	distance_file << blank << blank << distance <<
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

// M9.3.2: main VTU/PVTU writer (includes OutputStamp field data injection).
void write_solution(p4est_t *p4est, const IOAlgorithm::OutputStamp &stamp)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	int					ret;

#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}

#ifdef _WIN32
	const char* path_format = "output\\" P4EST_STRING "_Lagrangian_%04d";
#else
	const char* path_format = "output/"P4EST_STRING "_Lagrangian_%04d";
#endif

	snprintf(filename, BUFSIZ, path_format, stamp.file_step);

	numquads = p4est->local_num_quadrants;

	sc_array_t		*coord_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*coord_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*pressure_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*temperature_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*rho_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*internal_energy_array = sc_array_new_size(sizeof(double), numquads);

	vtu_cell_data_t		m_cell_data;
	m_cell_data.density_array = rho_array;
	m_cell_data.pressure_array = pressure_array;
	m_cell_data.temperature_array = temperature_array;
	m_cell_data.internal_energy_array = internal_energy_array;
	m_cell_data.coordx = coord_x_array;
	m_cell_data.coordy = coord_y_array;
	m_cell_data.velox = velo_x_array;
	m_cell_data.veloy = velo_y_array;


	p4est_iterate(p4est, NULL,
		&m_cell_data,
		IOCallbacks::quadrant_copy_variable_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);

	context = p4est_vtk_write_cell_dataf(
		context,
		0,
		1,
		1,
		0,
		3,
		0,
		"Pressure",
		pressure_array,
		"density",
		rho_array,
		"internal_energy",
		internal_energy_array,
		context
	);
	SC_CHECK_ABORT(context != NULL,
		P4EST_STRING "_vtk:Error:writing cell data");

	context = p4est_vtk_write_point_dataf(context, 4, 0,
		"NodeX", coord_x_array,
		"NodeY", coord_y_array,
		"NodeU", velo_x_array,
		"NodeV", velo_y_array,
		context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x_array);
	sc_array_destroy(coord_y_array);
	sc_array_destroy(velo_x_array);
	sc_array_destroy(velo_y_array);
	sc_array_destroy(pressure_array);
	sc_array_destroy(rho_array);
	sc_array_destroy(internal_energy_array);
	sc_array_destroy(temperature_array);


	if (p4est->mpirank == 0) {
		char pvtu_filename[1024];
		snprintf(pvtu_filename, sizeof(pvtu_filename), "%s.pvtu", filename);
		FILE *f = fopen(pvtu_filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *string = (char *)malloc(fsize + 1);
			fread(string, 1, fsize, f);
			fclose(f);
			string[fsize] = 0;

			char *insert_pos = strstr(string, "</VTKFile>");
			if (insert_pos) {
				*insert_pos = '\0';
				f = fopen(pvtu_filename, "wb");
				if (f) {
					fprintf(f, "%s", string);
					fprintf(f,
						"  <FieldData>\n"
						"    <DataArray type=\"Float64\" Name=\"TimeValue\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %.16g\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"FileStep\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"StateStep\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"    <DataArray type=\"Int32\" Name=\"OutputPhase\" NumberOfTuples=\"1\" format=\"ascii\">\n"
						"      %d\n"
						"    </DataArray>\n"
						"  </FieldData>\n</VTKFile>\n",
						stamp.time,
						stamp.file_step,
						stamp.state_step,
						IOAlgorithm::phase_code(stamp.phase));
					fclose(f);
				}
			}
			free(string);
		}
	}
}

// M9.3.2: debug checkpoint VTU writer and its array-copy callback.
void debug_quadrant_copy_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	debug_vtu_cell_data_t *m_cell_data = (debug_vtu_cell_data_t *)user_data;
	p4est_t *p4est = info->p4est;
	p4est_tree_t *tree = p4est_tree_array_index(p4est->trees, info->treeid);
	quad_data_t *quad_data = (quad_data_t *)info->quad->p.user_data;

	p4est_locidx_t local_id = info->quadid + tree->quadrants_offset;

	// Global SFC ID
	double global_id = MeshAdapter::global_sfc_id(p4est, local_id);
	*(double *)sc_array_index(m_cell_data->global_sfc_id_array, local_id) = global_id;

	CVariable *m_vara = (CVariable *)&quad_data->m_vara;

	*(double *)sc_array_index(m_cell_data->density_array, local_id) =
		m_vara->cell(idDensity_lag);
	*(double *)sc_array_index(m_cell_data->pressure_array, local_id) =
		m_vara->cell(idPressure_lag);
	*(double *)sc_array_index(m_cell_data->internal_energy_array, local_id) =
		m_vara->cell(idInternalEnergy_lag);

	// Corner pressures (using hdata[0].pi as proxy for half-edge pressure)
	*(double *)sc_array_index(m_cell_data->pressure_c0_array, local_id) = quad_data->m_cndata[0].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c1_array, local_id) = quad_data->m_cndata[1].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c2_array, local_id) = quad_data->m_cndata[2].hdata[0].pi;
	*(double *)sc_array_index(m_cell_data->pressure_c3_array, local_id) = quad_data->m_cndata[3].hdata[0].pi;

	// Corner velocities
	*(double *)sc_array_index(m_cell_data->velou_c0_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 0).x;
	*(double *)sc_array_index(m_cell_data->velou_c1_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 1).x;
	*(double *)sc_array_index(m_cell_data->velou_c2_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 2).x;
	*(double *)sc_array_index(m_cell_data->velou_c3_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 3).x;

	*(double *)sc_array_index(m_cell_data->velov_c0_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 0).y;
	*(double *)sc_array_index(m_cell_data->velov_c1_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 1).y;
	*(double *)sc_array_index(m_cell_data->velov_c2_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 2).y;
	*(double *)sc_array_index(m_cell_data->velov_c3_array, local_id) = m_vara->corner_vector(idcnVelocity_lag, 3).y;
}

void p4est_debug_output_vtu(p4est_t *p4est, const char *prefix, int step, int location_id)
{
	char filename[1024];
	snprintf(filename, sizeof(filename), "%s_checkpoint_%04d_loc%d", prefix, step, location_id);

	p4est_locidx_t numquads = p4est->local_num_quadrants;

	debug_vtu_cell_data_t m_cell_data;
	m_cell_data.global_sfc_id_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.density_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.internal_energy_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.pressure_c3_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velou_c3_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c0_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c1_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c2_array = sc_array_new_size(sizeof(double), numquads);
	m_cell_data.velov_c3_array = sc_array_new_size(sizeof(double), numquads);

	p4est_iterate(p4est, NULL, &m_cell_data, IOCallbacks::debug_quadrant_copy_variable_to_array_callback, NULL,
#ifdef P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_vtk_context_t *context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);

	context = p4est_vtk_write_cell_dataf(
		context, 1, 1, 1, 0,
		16, 0,
		"Global_SFC_ID", m_cell_data.global_sfc_id_array,
		"Density", m_cell_data.density_array,
		"Pressure", m_cell_data.pressure_array,
		"InternalEnergy", m_cell_data.internal_energy_array,
		"Pressure_c0", m_cell_data.pressure_c0_array,
		"Pressure_c1", m_cell_data.pressure_c1_array,
		"Pressure_c2", m_cell_data.pressure_c2_array,
		"Pressure_c3", m_cell_data.pressure_c3_array,
		"VelocityU_c0", m_cell_data.velou_c0_array,
		"VelocityU_c1", m_cell_data.velou_c1_array,
		"VelocityU_c2", m_cell_data.velou_c2_array,
		"VelocityU_c3", m_cell_data.velou_c3_array,
		"VelocityV_c0", m_cell_data.velov_c0_array,
		"VelocityV_c1", m_cell_data.velov_c1_array,
		"VelocityV_c2", m_cell_data.velov_c2_array,
		"VelocityV_c3", m_cell_data.velov_c3_array,
		context
	);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing cell data");

	int retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");

	sc_array_destroy(m_cell_data.global_sfc_id_array);
	sc_array_destroy(m_cell_data.pressure_array);
	sc_array_destroy(m_cell_data.density_array);
	sc_array_destroy(m_cell_data.internal_energy_array);
	sc_array_destroy(m_cell_data.pressure_c0_array);
	sc_array_destroy(m_cell_data.pressure_c1_array);
	sc_array_destroy(m_cell_data.pressure_c2_array);
	sc_array_destroy(m_cell_data.pressure_c3_array);
	sc_array_destroy(m_cell_data.velou_c0_array);
	sc_array_destroy(m_cell_data.velou_c1_array);
	sc_array_destroy(m_cell_data.velou_c2_array);
	sc_array_destroy(m_cell_data.velou_c3_array);
	sc_array_destroy(m_cell_data.velov_c0_array);
	sc_array_destroy(m_cell_data.velov_c1_array);
	sc_array_destroy(m_cell_data.velov_c2_array);
	sc_array_destroy(m_cell_data.velov_c3_array);
}

// M9.3.3: distance-profile writer (rank-local mkdir + iterate callback).
void write_distance_profiles(p4est_t *p4est)
{
	p4est_data_t		*p4est_data = (p4est_data_t*)p4est->user_pointer;
	int ret;
#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}
	p4est_iterate(p4est, NULL,
		(void *)p4est_data,
		IOCallbacks::quadrant_write_distance_profiles_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);
}

// M9.3.3: global total-energy conservation check (MPI Allreduce + abort gate).
void StatTotalEnergyError(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_data->total_energy_cur = 0.;
	p4est_data->total_energy_lag = 0.;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		IOCallbacks::quadrant_total_energy_error_callback,
		NULL,
#ifdef P4_TO_P8
		NULL,

#endif
		NULL);

	double local_energy_cur = p4est_data->total_energy_cur;
	double local_energy_lag = p4est_data->total_energy_lag;
	sc_MPI_Allreduce(&local_energy_cur, &p4est_data->total_energy_cur, 1, sc_MPI_DOUBLE, sc_MPI_SUM, p4est->mpicomm);
	sc_MPI_Allreduce(&local_energy_lag, &p4est_data->total_energy_lag, 1, sc_MPI_DOUBLE, sc_MPI_SUM, p4est->mpicomm);

	if (p4est_data->current_step == 1)
	{
		p4est_data->total_energy_init = p4est_data->total_energy_cur;
	}

	if (p4est->mpirank == 0) {
		std::ofstream &energy_file = energy_error_file();
		energy_file << blank << blank << p4est_data->current_time << blank << blank <<
			(p4est_data->total_energy_lag - p4est_data->total_energy_cur) /
			p4est_data->total_energy_cur << endl;
	}

	P4EST_GLOBAL_PRODUCTIONF("the total energy error is %#.16g\n", (p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init);
	if (abs((p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init) > 1e-6)
	{
		P4EST_GLOBAL_PRODUCTIONF("The total energy is not conservative after time step\n");
		abort();
	}
}
} // namespace IOCallbacks
