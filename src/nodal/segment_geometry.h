#pragma once

#include <cmath>
#include "nodal/nodal_storage.h"

// T3: Pure segment geometry, independent of p4est storage and solver algebra.
// A regular face has one segment; a hanging face has two segments that share
// a midpoint.  Endpoint metric weights are pure geometry values only; no
// master assembly or cylindrical solver activation happens here.

namespace Nodal {

struct SegmentGeometryError {
	bool failed;
	const char *reason;
};

inline SegmentGeometryError build_regular_segment(
	const double start[2], const double end[2],
	EdgeSegmentGeometry &out, int normal_sign = 1)
{
	const double dx = end[0] - start[0];
	const double dy = end[1] - start[1];
	const double length = std::sqrt(dx * dx + dy * dy);
	if (length <= 1e-12) {
		return SegmentGeometryError{true, "zero length segment"};
	}
	if (normal_sign != 1 && normal_sign != -1) {
		return SegmentGeometryError{true, "invalid normal sign"};
	}
	out.normal.x = normal_sign * (-dy) / length;
	out.normal.y = normal_sign * dx / length;
	out.length = length;
	out.endpoint_weights[0] = 0.5;
	out.endpoint_weights[1] = 0.5;
	return SegmentGeometryError{false, nullptr};
}

inline SegmentGeometryError build_split_segment_pair(
	const double start[2], const double mid[2], const double end[2],
	EdgeSegmentGeometry segments[2], int normal_sign = 1)
{
	SegmentGeometryError err = build_regular_segment(start, mid, segments[0], normal_sign);
	if (err.failed) {
		return err;
	}
	return build_regular_segment(mid, end, segments[1], normal_sign);
}

inline SegmentGeometryError validate_segment(const EdgeSegmentGeometry &segment)
{
	if (segment.length <= 1e-12) {
		return SegmentGeometryError{true, "zero length"};
	}
	const double normal_length =
		std::sqrt(segment.normal.x * segment.normal.x +
			segment.normal.y * segment.normal.y);
	if (std::fabs(normal_length - 1.0) > 1e-9) {
		return SegmentGeometryError{true, "non unit normal"};
	}
	if (segment.endpoint_weights[0] < 0.0 || segment.endpoint_weights[1] < 0.0) {
		return SegmentGeometryError{true, "negative endpoint weight"};
	}
	const double weight_sum =
		segment.endpoint_weights[0] + segment.endpoint_weights[1];
	if (std::fabs(weight_sum - 1.0) > 1e-9) {
		return SegmentGeometryError{true, "endpoint weights do not sum to one"};
	}
	return SegmentGeometryError{false, nullptr};
}

inline double total_segment_length(const EdgeSegmentGeometry segments[2])
{
	return segments[0].length + segments[1].length;
}

inline SegmentGeometryError check_split_closure(
	const double start[2], const double end[2],
	const EdgeSegmentGeometry segments[2])
{
	const double dx = end[0] - start[0];
	const double dy = end[1] - start[1];
	const double full = std::sqrt(dx * dx + dy * dy);
	if (std::fabs(total_segment_length(segments) - full) > 1e-9) {
		return SegmentGeometryError{true, "split lengths do not add to full length"};
	}
	return SegmentGeometryError{false, nullptr};
}

} // namespace Nodal