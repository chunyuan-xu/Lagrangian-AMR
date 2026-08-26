#pragma once

#include <cmath>
#include <cstdint>
#include "nodal/matrix_accessor.h"

// S1b: pure DBGF local algebra for one corner.  M_ck = Z*sum L*n⊗n,
// b_ck = M_ck*U_c + P*N_ck, branch force = p*L*n, physical corner force =
// b_ck - M_ck*U_k, and dissipation = Z*sum L*((U_k-U_c)·n)^2.

namespace Nodal {

struct HalfEdgeInput {
	double length;
	double normal[2];
};

struct LocalCornerInput {
	double z;
	double pressure;
	double u_c[2];
	double u_k[2];
	HalfEdgeInput e[2];
};

struct LocalCornerOutput {
	Matrix2 M;
	double N[2];
	double b[2];
	double branch_pressure[2];
	double branch_force[2][2];
	double physical_force[2];
	double dissipation;
};

inline LocalCornerOutput build_local_corner(const LocalCornerInput &in)
{
	LocalCornerOutput out;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			out.M.m[i][j] = 0.0;
		}
		out.N[0] = 0.0;
		out.N[1] = 0.0;
		out.b[0] = 0.0;
		out.b[1] = 0.0;
		out.branch_pressure[i] = 0.0;
		out.branch_force[i][0] = 0.0;
		out.branch_force[i][1] = 0.0;
	}
	out.physical_force[0] = 0.0;
	out.physical_force[1] = 0.0;
	out.dissipation = 0.0;

	for (int e = 0; e < 2; ++e) {
		const double L = in.e[e].length;
		const double nx = in.e[e].normal[0];
		const double ny = in.e[e].normal[1];
		const double delta_u_dot_n =
			(in.u_k[0] - in.u_c[0]) * nx + (in.u_k[1] - in.u_c[1]) * ny;

		// N and M
		out.N[0] += L * nx;
		out.N[1] += L * ny;
		out.M.m[0][0] += in.z * L * nx * nx;
		out.M.m[0][1] += in.z * L * nx * ny;
		out.M.m[1][0] += in.z * L * ny * nx;
		out.M.m[1][1] += in.z * L * ny * ny;

		out.branch_pressure[e] = in.pressure - in.z * delta_u_dot_n;
		out.branch_force[e][0] = out.branch_pressure[e] * L * nx;
		out.branch_force[e][1] = out.branch_pressure[e] * L * ny;
		out.dissipation += in.z * L * delta_u_dot_n * delta_u_dot_n;
	}

	out.b[0] = out.M.m[0][0] * in.u_c[0] + out.M.m[0][1] * in.u_c[1] +
		in.pressure * out.N[0];
	out.b[1] = out.M.m[1][0] * in.u_c[0] + out.M.m[1][1] * in.u_c[1] +
		in.pressure * out.N[1];

	out.physical_force[0] = out.b[0] -
		(out.M.m[0][0] * in.u_k[0] + out.M.m[0][1] * in.u_k[1]);
	out.physical_force[1] = out.b[1] -
		(out.M.m[1][0] * in.u_k[0] + out.M.m[1][1] * in.u_k[1]);

	return out;
}

} // namespace Nodal
