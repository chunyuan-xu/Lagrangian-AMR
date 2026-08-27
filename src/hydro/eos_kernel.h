#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"

// M14.11: pure EOS and sound-speed update kernels.
namespace HydroCallbacks {

inline void update_eos(CVariable &vara)
{
	vara.cell(idPressure_lag) = PhysicalAlg::EquationOfState(
		vara.cell(idGamma),
		vara.cell(idDensity_lag),
		vara.cell(idInternalEnergy_lag));
	if (vara.cell(idPressure_lag) <= m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("the value of pressure is illegal\n");
	}
}

inline void update_sound_speed(CVariable &vara)
{
	vara.cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
		vara.cell(idGamma),
		vara.cell(idPressure_lag),
		vara.cell(idDensity_lag));
}

} // namespace HydroCallbacks
