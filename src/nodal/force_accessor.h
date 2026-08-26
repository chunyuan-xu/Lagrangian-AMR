#pragma once

#include <cstdint>
#include "nodal/nodal_storage.h"
#include "nodal/topology_mapping.h"

// L6c: regular-force accessor audit.  Reads the same weighted face geometry
// as the matrix accessor and reproduces the corner force geometry (Lcp*Ncp).

namespace Nodal {

constexpr std::uint8_t kCornerIncidentFaceA[kQuadCornerCount] = {0, 0, 1, 1};
constexpr std::uint8_t kCornerIncidentFaceB[kQuadCornerCount] = {2, 3, 3, 2};
constexpr std::uint8_t kCornerIncidentFaceAEndpoint[kQuadCornerCount] = {0, 1, 1, 0};
constexpr std::uint8_t kCornerIncidentFaceBEndpoint[kQuadCornerCount] = {0, 0, 1, 1};

inline Vec2Storage segment_geometry_force(const EdgeSegmentGeometry &seg,
	std::uint8_t endpoint)
{
	const double weight = endpoint < 2 ? seg.endpoint_weights[endpoint] : 0.0;
	const double scale = seg.length * weight;
	Vec2Storage out;
	out.x = scale * seg.normal.x;
	out.y = scale * seg.normal.y;
	return out;
}

inline Vec2Storage corner_geometry_force(const FaceData faces[4], Corner corner)
{
	const std::uint8_t c = static_cast<std::uint8_t>(corner);
	Vec2Storage out = segment_geometry_force(faces[kCornerIncidentFaceA[c]].segments[0],
		kCornerIncidentFaceAEndpoint[c]);
	const Vec2Storage b = segment_geometry_force(faces[kCornerIncidentFaceB[c]].segments[0],
		kCornerIncidentFaceBEndpoint[c]);
	out.x += b.x;
	out.y += b.y;
	return out;
}

} // namespace Nodal
