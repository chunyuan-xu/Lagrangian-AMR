#pragma once

#include <cstdint>
#include <cmath>
#include "nodal/nodal_storage.h"
#include "nodal/topology_mapping.h"

// L6b: regular-matrix accessor audit.  The DBGF matrix accessor reads the new
// FaceData geometry and reproduces the same outer-product corner matrix that
// the legacy reader builds from half-edge Ncp/Lcp.  Zcp/Rcp scaling is
// intentionally left to later S-phase kernels; this audit freezes geometry.

namespace Nodal {

struct Matrix2 {
	double m[2][2];
};

inline constexpr std::uint8_t kCornerIncidentFaceA[kQuadCornerCount] = {0, 0, 1, 1};
inline constexpr std::uint8_t kCornerIncidentFaceB[kQuadCornerCount] = {2, 3, 3, 2};
inline constexpr std::uint8_t kCornerIncidentFaceAEndpoint[kQuadCornerCount] = {0, 1, 1, 0};
inline constexpr std::uint8_t kCornerIncidentFaceBEndpoint[kQuadCornerCount] = {0, 0, 1, 1};

inline Matrix2 dyadic_product(double x, double y)
{
	Matrix2 out;
	out.m[0][0] = x * x;
	out.m[0][1] = x * y;
	out.m[1][0] = y * x;
	out.m[1][1] = y * y;
	return out;
}

inline Matrix2 segment_geometry_matrix_weighted(const EdgeSegmentGeometry &seg,
	std::uint8_t endpoint)
{
	Matrix2 out = dyadic_product(seg.normal.x, seg.normal.y);
	const double weight = endpoint < 2 ? seg.endpoint_weights[endpoint] : 0.0;
	const double scale = seg.length * weight;
	out.m[0][0] *= scale;
	out.m[0][1] *= scale;
	out.m[1][0] *= scale;
	out.m[1][1] *= scale;
	return out;
}

inline Matrix2 corner_geometry_matrix(const FaceData faces[4], Corner corner)
{
	const std::uint8_t c = static_cast<std::uint8_t>(corner);
	Matrix2 out = segment_geometry_matrix_weighted(
		faces[kCornerIncidentFaceA[c]].segments[0], kCornerIncidentFaceAEndpoint[c]);
	const Matrix2 b = segment_geometry_matrix_weighted(
		faces[kCornerIncidentFaceB[c]].segments[0], kCornerIncidentFaceBEndpoint[c]);
	out.m[0][0] += b.m[0][0];
	out.m[0][1] += b.m[0][1];
	out.m[1][0] += b.m[1][0];
	out.m[1][1] += b.m[1][1];
	return out;
}

} // namespace Nodal
