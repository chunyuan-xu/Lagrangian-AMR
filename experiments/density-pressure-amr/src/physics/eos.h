#pragma once
#include <cmath>

namespace PhysicalAlg {


inline double EquationOfState(const double &gamma, const double &density, const double &internal_energy)
{
	return (gamma - 1.0) * density * internal_energy;
}


inline double CalculateSoundSpeed(const double &gamma, const double &pressure, const double &density)
{
	if (density <= 0.0 || pressure <= 0.0) {
		return 0.0;
	}
	return std::sqrt(gamma * pressure / density);
}


inline double CalculateCellMass(const double &volume, const double &density)
{
	return volume * density;
}

} 
