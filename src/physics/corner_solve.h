#pragma once
#include <cmath>
#include <cstdio>
#include "defines.h"
#include "core/vector_matrix.h"

// M5.1: CornerSolve — pure 2x2 corner boundary solve extracted from
// main.cpp. Inputs are the two boundary-info objects, the assembled
// matrix, and the RHS; output is the corner velocity. All boundary
// branches, thresholds, and formulas are byte-identical to the original
// BoundaryNodeVelocityComputation.

namespace CornerSolve {

inline CDoubleVector boundary_node_velocity(const CPointBounInfo &BounPlus,
	const CPointBounInfo &BounMinus, const CDoubleMatrix MatrixP,
	const CDoubleVector m_RHS)
{
	int				enumPlus = BounPlus.enumType;
	int				enumMinus = BounMinus.enumType;
	double			ValPlus = BounPlus.Val;
	double			ValMinus = BounMinus.Val;
	CDoubleVector	NcpPlus = BounPlus.Ncp;
	CDoubleVector	NcpMinus = BounMinus.Ncp;
	double			LcpPlus = BounPlus.Lcp;
	double			LcpMinus = BounMinus.Lcp;
	CDoubleVector	velocity = CDoubleVector(0., 0.);


	if (VelocityBoundary == enumPlus || VelocityBoundary == enumMinus
		||CircleCenterBoundary == enumPlus || CircleCenterBoundary == enumMinus) { return velocity; }
	CDoubleMatrix MatrixPInverse;
	MatrixPInverse = GeometryAlg::MatrixInverse(MatrixP);

	double aa, bb, cc, dd, na, nb, ma, mb, ma1, mb1, PIStar, cos_theta, sin_theta;

	aa = MatrixPInverse.xx;
	bb = MatrixPInverse.xy;
	cc = MatrixPInverse.yx;
	dd = MatrixPInverse.yy;
	na = m_RHS.x;
	nb = m_RHS.y;
	ma = NcpPlus.x * LcpPlus;
	ma1 = NcpMinus.x * LcpMinus;
	mb = NcpPlus.y * LcpPlus;
	mb1 = NcpMinus.y * LcpMinus;
	cos_theta = NcpPlus.x;
	sin_theta = NcpPlus.y;

	bool IsSolved, IsColinear;
	IsSolved = false;


	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.x = aa * (ma*ValPlus + ma1*ValMinus + na) +
				bb*(mb*ValPlus + mb1*ValMinus + nb);
			velocity.y = cc* (ma*ValPlus + ma1*ValMinus + na) +
				dd*(mb*ValPlus + mb1*ValMinus + nb);
			IsSolved = true;
		}
	}


	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			IsColinear = false;
			if (fabs(NcpPlus.x * NcpMinus.y - NcpPlus.y * NcpMinus.x) < 1e-10) { IsColinear = true; }


			if (IsColinear)
			{

				if (fabs(NcpPlus.x) < 1e-12)
				{
					velocity.y = ValPlus;
					if (fabs(ma*cc + mb*dd) > 1e-12)
					{
						PIStar = (ValPlus - na*cc - nb*dd) / (ma*cc + mb*dd);
					}
					else
					{
						PIStar = 0.0;
					}
					velocity.x = aa*(ma*PIStar + na) + bb*(mb*PIStar + nb);
				}


				else if (fabs(NcpPlus.y) < 1e-12)
				{
					velocity.x = ValPlus;
					if (fabs(ma*aa + mb*bb) > 1e-12)
					{
						PIStar = (ValPlus - na*aa - nb*bb) / (ma*aa + mb*bb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.y = cc*(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
				else
				{


					if (fabs(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb) > 1e-12)
					{
						PIStar = (ValPlus - cos_theta*aa*na - cos_theta*bb*nb - sin_theta*cc*na - sin_theta*dd*nb) /
							(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.x = aa *(ma*PIStar + na) + bb*(mb*PIStar + nb);
					velocity.y = cc *(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
			}
			else
			{

				velocity = ValPlus*NcpPlus + ValMinus*NcpMinus;
			}
		}
		IsSolved = true;
	}


	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			if (fabs(NcpMinus.x) < 1e-12)
			{
				velocity.y = ValMinus;
				PIStar = (velocity.y - cc*(na + ma*ValPlus) - dd*(nb + mb*ValPlus)) /
					(cc*ma1 + dd*mb1);
				velocity.x = aa*(na + ma1*PIStar + ma*ValPlus) +
					bb*(nb + mb1*PIStar + mb*ValPlus);
			}
			else if (fabs(NcpMinus.y) < 1e-12)
			{
				velocity.x = ValMinus;
				PIStar = (velocity.x - aa*(na + ma*ValPlus) - bb*(nb + mb*ValPlus)) /
					(aa*ma1 + bb*mb1);
				velocity.y = cc*(na + ma1*PIStar + ma*ValPlus) +
					dd*(nb + mb1*PIStar + mb*ValPlus);
			}
		}
		IsSolved = true;
	}


	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			if (fabs(NcpPlus.x) < 1e-12)
			{
				velocity.y = ValPlus;
				PIStar = (velocity.y - cc*(na + ma1*ValMinus) - dd*(nb + mb1*ValMinus)) /
					(cc*ma + dd*mb);
				velocity.x = aa*(na + ma*PIStar + ma1*ValMinus) +
					bb*(nb + mb*PIStar + mb1*ValMinus);
			}
			else if (fabs(NcpPlus.y) < 1e-12)
			{
				velocity.x = ValPlus;
				PIStar = (velocity.x - aa*(na + ma1*ValMinus) - bb*(nb + mb1*ValMinus)) /
					(aa*ma + bb*mb);
				velocity.y = cc*(na + ma*PIStar + ma1*ValMinus) +
					dd*(nb + mb*PIStar + mb1*ValMinus);
			}
		}
		IsSolved = true;
	}


	if (enumPlus == SymmetryBoundary || enumMinus == SymmetryBoundary)
	{
		velocity.y = 0.;
		velocity.x = na / MatrixP.xx;
		IsSolved = true;
	}


	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValMinus * NcpMinus.x;
			IsSolved = true;
		}
	}


	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValPlus * NcpPlus.x;
			IsSolved = true;
		}
	}


	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValPlus*ma) / MatrixP.xx;
			IsSolved = true;
		}
	}


	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValMinus*ma1) / MatrixP.xx;
			IsSolved = true;
		}
	}

	if (!IsSolved)
	{
		printf("Boundary condition is not solved\n");
	}
	return velocity;
}

} // namespace CornerSolve
