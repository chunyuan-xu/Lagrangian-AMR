#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include "nodal/nodal_storage.h"
#include "nodal/segment_geometry.h"
#include "nodal/topology_mapping.h"

// L3: Regular planar geometry mirror.  One FaceData holds a regular face's
// conforming one-segment geometry plus endpoint metric weights.  These
// helpers are pure and do not depend on quad_data_t/defines.h.

namespace Nodal {

struct FaceGeometryMirrorError {
	bool failed;
	const char *reason;
};

inline constexpr std::uint8_t kFaceSegment0Present = 0x01;
inline constexpr std::uint8_t kFaceSegment1Present = 0x02;
inline constexpr std::uint8_t kFaceHangingFlag = 0x04;

inline std::uint32_t make_face_logical_header(Face face,
	std::uint8_t endpoint0, std::uint8_t endpoint1)
{
	return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(face)) << 16) |
		(static_cast<std::uint32_t>(endpoint0) << 8) |
		static_cast<std::uint32_t>(endpoint1);
}

inline FaceGeometryMirrorError build_regular_face(FaceData &out,
	Face face, const std::uint8_t endpoints[2],
	const double normal[2], double length_a, double length_b,
	double rcp_a, double rcp_b)
{
	std::memset(&out, 0, sizeof(out));
	if (endpoints[0] >= kQuadCornerCount || endpoints[1] >= kQuadCornerCount) {
		return FaceGeometryMirrorError{true, "invalid face endpoint"};
	}
	if (length_a <= 0.0 || length_b <= 0.0) {
		return FaceGeometryMirrorError{true, "non-positive half-length"};
	}
	if (!(rcp_a > 0.0) || !(rcp_b > 0.0) ||
		!std::isfinite(rcp_a) || !std::isfinite(rcp_b)) {
		return FaceGeometryMirrorError{true, "non-positive endpoint weight"};
	}

	out.flags = kFaceSegment0Present;
	out.logical_header = make_face_logical_header(face, endpoints[0], endpoints[1]);

	const double n_len = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1]);
	if (n_len <= 1e-12) {
		return FaceGeometryMirrorError{true, "zero face normal"};
	}
	out.segments[0].normal.x = normal[0] / n_len;
	out.segments[0].normal.y = normal[1] / n_len;
	out.segments[0].length = length_a + length_b;
	const double weight_sum = rcp_a + rcp_b;
	out.segments[0].endpoint_weights[0] = rcp_a / weight_sum;
	out.segments[0].endpoint_weights[1] = rcp_b / weight_sum;
	return FaceGeometryMirrorError{false, nullptr};
}

inline FaceGeometryMirrorError verify_regular_face(const FaceData &record,
	Face face, const std::uint8_t endpoints[2],
	const double normal[2], double length_a, double length_b,
	double rcp_a, double rcp_b)
{
	if ((record.flags & kFaceSegment0Present) == 0) {
		return FaceGeometryMirrorError{true, "missing segment 0"};
	}
	if ((record.flags & kFaceSegment1Present) != 0) {
		return FaceGeometryMirrorError{true, "unexpected segment 1"};
	}
	if (record.logical_header != make_face_logical_header(face, endpoints[0], endpoints[1])) {
		return FaceGeometryMirrorError{true, "logical header endpoint mismatch"};
	}
	FaceData expected;
	FaceGeometryMirrorError err = build_regular_face(expected, face, endpoints,
		normal, length_a, length_b, rcp_a, rcp_b);
	if (err.failed) {
		return err;
	}
	if (record.segments[0].length != expected.segments[0].length ||
		std::fabs(record.segments[0].normal.x - expected.segments[0].normal.x) > 1e-9 ||
		std::fabs(record.segments[0].normal.y - expected.segments[0].normal.y) > 1e-9 ||
		std::fabs(record.segments[0].endpoint_weights[0] - expected.segments[0].endpoint_weights[0]) > 1e-9 ||
		std::fabs(record.segments[0].endpoint_weights[1] - expected.segments[0].endpoint_weights[1]) > 1e-9) {
		return FaceGeometryMirrorError{true, "face geometry mismatch"};
	}
	return FaceGeometryMirrorError{false, nullptr};
}

inline FaceGeometryMirrorError verify_regular_cell_closure(
	const FaceData faces[4])
{
	double sx = 0.0;
	double sy = 0.0;
	for (int f = 0; f < 4; ++f) {
		sx += faces[f].segments[0].length * faces[f].segments[0].normal.x;
		sy += faces[f].segments[0].length * faces[f].segments[0].normal.y;
	}
	if (std::fabs(sx) > 1e-9 || std::fabs(sy) > 1e-9) {
		return FaceGeometryMirrorError{true, "cell closure mismatch"};
	}
	return FaceGeometryMirrorError{false, nullptr};
}

} // namespace Nodal
