#pragma once
#include <cmath>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "core/vector_matrix.h"

// M14.1: per-corner local matrix/RHS algebra extracted from the callback.
namespace HydroCallbacks {

inline void build_corner_matrix_rhs(CVariable &vara, CCorner_data &cndata,
	int coord_type, int solver_type, int k)
{
	CHalf_edge_data &m_plus =
		cndata.hdata[CHalf_edge_data::cside::plus];
	CHalf_edge_data &m_minus =
		cndata.hdata[CHalf_edge_data::cside::minus];

	vara.corner(idReconstructDensity, k) = vara.cell(idDensity_cur);
	vara.corner(idReconstructPressure, k) = vara.cell(idPressure_cur);
	vara.corner_vector(idReconstructVelocity, k) =
		vara.cell_vector(idCentroidVelo_cur);

	const CDoubleVector DeltaU =
		vara.corner_vector(idcnVelocity_lag, k) -
		vara.corner_vector(idReconstructVelocity, k);
	const double abs_deltau = std::sqrt(DeltaU.x * DeltaU.x + DeltaU.y * DeltaU.y);
	const CDoubleVector LcpNcp =
		m_plus.Lcp * m_plus.Ncp + m_minus.Lcp * m_minus.Ncp;
	const CDoubleVector LcpNcpPc =
		LcpNcp * vara.corner(idReconstructPressure, k);

	if (coord_type == p4est_data_t::MyCoordType::plane) {
		// RcpLcpNcpPc is computed but not used by later consumers in the
		// current callback; kept for source parity in the local block.
		CDoubleVector RcpLcpNcpPc =
			m_plus.Rcp * m_plus.Lcp * m_plus.Ncp +
			m_minus.Rcp * m_minus.Lcp * m_minus.Ncp;
		(void)RcpLcpNcpPc;
	}

	m_plus.Zcp = vara.corner(idReconstructDensity, k) * vara.cell(idSoundSpeed);
	m_minus.Zcp = vara.corner(idReconstructDensity, k) * vara.cell(idSoundSpeed);
	m_plus.delta_u_cp = DeltaU;
	m_minus.delta_u_cp = DeltaU;
	m_plus.Uc_cur = vara.corner_vector(idReconstructVelocity, k);
	m_minus.Uc_cur = vara.corner_vector(idReconstructVelocity, k);
	m_plus.pi = vara.corner(idReconstructPressure, k) -
		m_plus.Zcp * (m_plus.delta_u_cp ^ m_plus.Ncp);
	m_minus.pi = vara.corner(idReconstructPressure, k) -
		m_minus.Zcp * (m_minus.delta_u_cp ^ m_minus.Ncp);

	const CDoubleMatrix NcpPlusMatrix =
		GeometryAlg::DyadicProduct(m_plus.Ncp, m_plus.Ncp);
	const CDoubleMatrix NcpMinusMatrix =
		GeometryAlg::DyadicProduct(m_minus.Ncp, m_minus.Ncp);

	vara.MarCnData[idcnMcp][k] =
		m_plus.Zcp * m_plus.Rcp * m_plus.Lcp * NcpPlusMatrix +
		m_minus.Zcp * m_minus.Rcp * m_minus.Lcp * NcpMinusMatrix;

	if (solver_type == p4est_data_t::RiemannSolver::Rotated &&
		abs_deltau > m_eps) {
		vara.MarCnData[idcnMcp][k].xx =
			m_plus.Zcp * m_plus.Rcp * m_plus.Lcp *
				std::abs(DeltaU ^ m_plus.Ncp) / abs_deltau +
			m_minus.Zcp * m_minus.Rcp * m_minus.Lcp *
				std::abs(DeltaU ^ m_minus.Ncp) / abs_deltau;
		vara.MarCnData[idcnMcp][k].yy =
			vara.MarCnData[idcnMcp][k].xx;
		vara.MarCnData[idcnMcp][k].xy = 0.;
		vara.MarCnData[idcnMcp][k].yx = 0.;
	}

	vara.corner_vector(idcnMcpUc, k) = GeometryAlg::MatrixDotVector(
		vara.MarCnData[idcnMcp][k],
		vara.corner_vector(idReconstructVelocity, k));
	vara.corner_vector(idcnRHS, k) =
		LcpNcpPc + vara.corner_vector(idcnMcpUc, k);
}

} // namespace HydroCallbacks
