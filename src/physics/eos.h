#pragma once
#include <cmath>

namespace PhysicalAlg {

// Ideal Gas Equation of State: P = (gamma - 1) * density * internal_energy
inline double EquationOfState(const double &gamma, const double &density, const double &internal_energy)
{
	return (gamma - 1.0) * density * internal_energy;
}

// Sound speed: c = sqrt(gamma * P / density)
inline double CalculateSoundSpeed(const double &gamma, const double &pressure, const double &density)
{
	if (density <= 0.0 || pressure <= 0.0) {
		return 0.0;
	}
	return std::sqrt(gamma * pressure / density);
}

// Cell mass: m = volume * density
inline double CalculateCellMass(const double &volume, const double &density)
{
	return volume * density;
}

} // namespace PhysicalAlg
