#pragma once
#include <cmath>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "variable.h"

// M14.10: pure total/internal energy update kernel.
namespace HydroCallbacks {

inline void update_energy(
	CVariable &vara,
	double dt_iter,
	int which_case,
	int quadid)
{
	vara.cell(idTotalEnergy_lag) =
		vara.cell(idTotalEnergy_half)
		- dt_iter * vara.cell(idTotalWork) / vara.cell(idMass);

	double source = 0.;
	if (which_case == ProblemNo::TaylorGreen)
	{
		source = dt_iter * 5. * M_PI / 8. * vara.cell(idVolume) *
			(std::cos(3. * M_PI * vara.cell_vector(idCentroidCoord_lag).x) *
				std::cos(M_PI * vara.cell_vector(idCentroidCoord_lag).y) -
			 std::cos(M_PI * vara.cell_vector(idCentroidCoord_lag).x) *
			 std::cos(3. * M_PI * vara.cell_vector(idCentroidCoord_lag).y)) /
			vara.cell(idMass);
	}

	if (vara.cell(idTotalEnergy_lag) <= m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF(
			"the total energy of quad %d is negative!\n", quadid);
		std::abort();
	}

	vara.cell(idInternalEnergy_lag) =
		vara.cell(idInternalEnergy_half) - dt_iter *
		(vara.cell(idTotalWork) - vara.cell(idKineticVariation))
		/ vara.cell(idMass);
	vara.cell(idInternalEnergy_lag) += source;
	if (vara.cell(idInternalEnergy_lag) <= m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF(
			"the total energy of quad %d is negative!\n", quadid);
		std::abort();
	}
}

} // namespace HydroCallbacks
