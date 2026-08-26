#pragma once

#include "defines.h"
#include "nodal/local_algebra.h"
#include "nodal/matrix_accessor.h"

// S2a: write cell-local DBGF master shadow contributions.  For each direct
// corner, build M,b from the mirrored FaceData and legacy cell state and
// store them in the inert CellNodalData.master block-diagonal layout.

namespace Nodal {

inline LocalCornerInput corner_input_from_faces(const quad_data_t &data,
	Corner corner, const FaceData faces[4])
{
	const std::uint8_t c = static_cast<std::uint8_t>(corner);
	LocalCornerInput in;
	in.z = data.m_vara.cell(idDensity_cur) * data.m_vara.cell(idSoundSpeed);
	in.pressure = data.m_vara.cell(idPressure_cur);
	in.u_c[0] = data.m_vara.cell_vector(idCentroidVelo_cur).x;
	in.u_c[1] = data.m_vara.cell_vector(idCentroidVelo_cur).y;
	in.u_k[0] = in.u_c[0];
	in.u_k[1] = in.u_c[1];

	const EdgeSegmentGeometry &seg_a =
		faces[kCornerIncidentFaceA[c]].segments[0];
	const EdgeSegmentGeometry &seg_b =
		faces[kCornerIncidentFaceB[c]].segments[0];
	in.e[0].length = seg_a.length * seg_a.endpoint_weights[kCornerIncidentFaceAEndpoint[c]];
	in.e[0].normal[0] = seg_a.normal.x;
	in.e[0].normal[1] = seg_a.normal.y;
	in.e[1].length = seg_b.length * seg_b.endpoint_weights[kCornerIncidentFaceBEndpoint[c]];
	in.e[1].normal[0] = seg_b.normal.x;
	in.e[1].normal[1] = seg_b.normal.y;
	return in;
}

inline void write_cell_local_master(quad_data_t &data)
{
	for (int c = 0; c < kQuadCornerCount; ++c) {
		LocalCornerInput in = corner_input_from_faces(data,
			static_cast<Corner>(c), data.nodal.faces);
		LocalCornerOutput out = build_local_corner(in);
		const int row = 2 * c;
		data.nodal.master.M[row][row] = out.M.m[0][0];
		data.nodal.master.M[row][row + 1] = out.M.m[0][1];
		data.nodal.master.M[row + 1][row] = out.M.m[1][0];
		data.nodal.master.M[row + 1][row + 1] = out.M.m[1][1];
		data.nodal.master.b[row] = out.b[0];
		data.nodal.master.b[row + 1] = out.b[1];
	}
}

} // namespace Nodal
