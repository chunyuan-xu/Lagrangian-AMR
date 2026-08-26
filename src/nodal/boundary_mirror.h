#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include "nodal/nodal_storage.h"
#include "nodal/topology_mapping.h"

// L2: Boundary mirror.  A BoundaryRecord holds one regular face's legacy
// boundary classification plus deterministic endpoint order.  These helpers
// are pure and do not depend on quad_data_t/defines.h so the layout and
// equality contract can be tested before any production writer is wired.

namespace Nodal {

struct BoundaryMirrorError {
	bool failed;
	const char *reason;
};

// Legacy boundary enums use -1 for InnerBoundary and 1..6 for the positive
// boundary types (see variable.h).  The wire record stores 0 for none/inner
// and the legacy positive value unchanged.
inline std::uint8_t boundary_type_to_record(int legacy_type)
{
	if (legacy_type == -1 || legacy_type == 0) {
		return 0;
	}
	if (legacy_type < 0 || legacy_type > 255) {
		return 0xFF;
	}
	return static_cast<std::uint8_t>(legacy_type);
}

inline int boundary_type_from_record(std::uint8_t record_type)
{
	if (record_type == 0) {
		return -1;
	}
	return static_cast<int>(record_type);
}

inline bool boundary_type_active(int legacy_type)
{
	return legacy_type != -1 && legacy_type != 0;
}

inline BoundaryMirrorError build_face_boundary(BoundaryRecord &out,
	Face face, const std::uint8_t endpoints[2],
	int type_a, double value_a,
	const double normal_a[2], double length_a,
	int type_b, double value_b,
	const double normal_b[2], double length_b)
{
	(void)face;
	std::memset(&out, 0, sizeof(out));
	if (endpoints[0] >= kQuadCornerCount || endpoints[1] >= kQuadCornerCount) {
		return BoundaryMirrorError{true, "invalid face endpoint"};
	}
	out.constraint_order[0] = endpoints[0];
	out.constraint_order[1] = endpoints[1];

	const bool boundary_a = boundary_type_active(type_a);
	const bool boundary_b = boundary_type_active(type_b);
	if (!boundary_a && !boundary_b) {
		out.type = 0;
		out.value = 0.0;
		out.normal.x = 0.0;
		out.normal.y = 0.0;
		out.length = 0.0;
		return BoundaryMirrorError{false, nullptr};
	}
	if (boundary_a != boundary_b) {
		return BoundaryMirrorError{true, "face boundary only on one endpoint"};
	}
	if (type_a != type_b) {
		return BoundaryMirrorError{true, "face endpoint boundary types differ"};
	}
	if (std::fabs(value_a - value_b) > 1e-12) {
		return BoundaryMirrorError{true, "face endpoint boundary values differ"};
	}

	const std::uint8_t record_type = boundary_type_to_record(type_a);
	if (record_type == 0xFF) {
		return BoundaryMirrorError{true, "invalid legacy boundary type"};
	}
	out.type = record_type;
	out.value = value_a;

	const double na = std::sqrt(normal_a[0] * normal_a[0] + normal_a[1] * normal_a[1]);
	const double nb = std::sqrt(normal_b[0] * normal_b[0] + normal_b[1] * normal_b[1]);
	if (na <= 1e-12 || nb <= 1e-12) {
		return BoundaryMirrorError{true, "zero endpoint normal"};
	}
	// A straight face must have the same outward unit normal at both ends.
	if (std::fabs(normal_a[0] - normal_b[0]) > 1e-9 ||
		std::fabs(normal_a[1] - normal_b[1]) > 1e-9) {
		return BoundaryMirrorError{true, "face endpoint normals differ"};
	}
	out.normal.x = normal_a[0] / na;
	out.normal.y = normal_a[1] / na;
	out.length = length_a + length_b;
	if (!(out.length > 0.0) || !std::isfinite(out.length)) {
		return BoundaryMirrorError{true, "non-positive face length"};
	}
	return BoundaryMirrorError{false, nullptr};
}

inline BoundaryMirrorError verify_face_boundary(const BoundaryRecord &record,
	Face face, const std::uint8_t endpoints[2],
	int type_a, double value_a,
	const double normal_a[2], double length_a,
	int type_b, double value_b,
	const double normal_b[2], double length_b)
{
	(void)face;
	if (record.constraint_order[0] != endpoints[0] ||
		record.constraint_order[1] != endpoints[1]) {
		return BoundaryMirrorError{true, "constraint order mismatch"};
	}
	BoundaryRecord expected;
	BoundaryMirrorError err = build_face_boundary(expected, face, endpoints,
		type_a, value_a, normal_a, length_a,
		type_b, value_b, normal_b, length_b);
	if (err.failed) {
		return err;
	}
	if (record.type != expected.type ||
		std::fabs(record.value - expected.value) > 1e-12 ||
		std::fabs(record.normal.x - expected.normal.x) > 1e-9 ||
		std::fabs(record.normal.y - expected.normal.y) > 1e-9 ||
		std::fabs(record.length - expected.length) > 1e-9) {
		return BoundaryMirrorError{true, "boundary record mismatch"};
	}
	return BoundaryMirrorError{false, nullptr};
}

} // namespace Nodal
