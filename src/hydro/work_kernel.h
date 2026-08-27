#pragma once
#include <cmath>
#include "defines.h"
#include "variable.h"

// M14.9: pure corner-work update kernel.
namespace HydroCallbacks {

inline void update_work(
	CVariable &vara,
	ParentBounInfo const *pc_info,
	int coordinate_type)
{
	vara.cell(idKineticVariation) = 0.;
	vara.cell(idTotalWork) = 0.;

	double m_alpha = 1.;
	double m_beta = 1.;
	if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
	{
		m_alpha = 2. * M_PI;
		m_beta = 2. * M_PI * vara.cell_vector(idCentroidCoord_cur).y;
	}

	CDoubleVector Velo = 0.5 * (
		vara.cell_vector(idCentroidVelo_half)
		+ vara.cell_vector(idCentroidVelo_lag));

	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		if (coordinate_type == p4est_data_t::MyCoordType::plane)
		{
			vara.cell(idKineticVariation) += m_beta * (
				Velo ^ (vara.corner_vector(idcnFcp, cnid)
					+ vara.corner_vector(idcnFluxRelaxed, cnid)));
		}
		if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
		{
			vara.cell(idKineticVariation) += m_beta * (
				Velo ^ vara.corner_vector(idAWFcp, cnid));
		}

		vara.cell(idTotalWork) += m_alpha * (
			vara.corner_vector(idcnVelocity_lag, cnid) ^ (
				vara.corner_vector(idcnFcp, cnid)
				+ vara.corner_vector(idcnFluxRelaxed, cnid)));
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		if (pc_info[eind].IsParentChildBoun == true)
		{
			if (coordinate_type == p4est_data_t::MyCoordType::plane)
			{
				vara.cell(idKineticVariation) += m_beta * (
					Velo ^ (vara.corner_vector(ideFcp, eind)
						+ pc_info[eind].FluxRelaxed));
			}
			vara.cell(idTotalWork) += m_alpha * (
				pc_info[eind].Hanging_velocity ^ (
					vara.corner_vector(ideFcp, eind)
					+ pc_info[eind].FluxRelaxed));
		}
	}
}

} // namespace HydroCallbacks
