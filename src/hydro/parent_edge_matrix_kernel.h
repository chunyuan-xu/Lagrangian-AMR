#pragma once
#include <cmath>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "core/vector_matrix.h"

// M14.3: parent-edge matrix/RHS algebra extracted from the callback.
namespace HydroCallbacks {

inline void build_parent_edge_matrix_rhs(CVariable &vara, ParentBounInfo &info,
	int k)
{
	vara.corner(idReconstructDensity, k) = vara.cell(idDensity_cur);
	vara.corner(idReconstructPressure, k) = vara.cell(idPressure_cur);
	vara.corner_vector(idReconstructVelocity, k) =
		vara.cell_vector(idCentroidVelo_cur);

	const CDoubleVector DeltaU =
		info.Hanging_velocity - vara.corner_vector(idReconstructVelocity, k);
	const CDoubleVector LcpNcp =
		info.Lcp[0] * info.Ncp[0] + info.Lcp[1] * info.Ncp[1];
	const CDoubleVector LcpNcpPc =
		LcpNcp * vara.corner(idReconstructPressure, k);
	const double Divergence = LcpNcpPc ^ DeltaU;

	// Tc is computed but not stored; source parity only.
	double Tc = 0.;
	if (Divergence < -1e-10) { Tc = 1.44; }
	(void)Tc;

	info.Zcp = vara.corner(idReconstructDensity, k) * vara.cell(idSoundSpeed);
	const CDoubleMatrix NcpPlusMatrix =
		GeometryAlg::DyadicProduct(info.Ncp[0], info.Ncp[0]);
	const CDoubleMatrix NcpMinusMatrix =
		GeometryAlg::DyadicProduct(info.Ncp[1], info.Ncp[1]);

	vara.MarCnData[ideMcp][k] =
		info.Zcp * info.Lcp[0] * NcpPlusMatrix +
		info.Zcp * info.Lcp[1] * NcpMinusMatrix;
	vara.corner_vector(ideMcpUc, k) = GeometryAlg::MatrixDotVector(
		vara.MarCnData[ideMcp][k],
		vara.corner_vector(idReconstructVelocity, k));
	vara.corner_vector(ideRHS, k) =
		LcpNcpPc + vara.corner_vector(ideMcpUc, k);
}

} // namespace HydroCallbacks
