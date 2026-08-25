#pragma once

#include <cstdint>
#include <cstring>
#include <utility>

// T2: Pure topology and orientation mapping.  No p4est storage is required.
// Keys are pure diagnostic values only; they are not global production
// node/face structures.

namespace Nodal {

enum class Face : std::uint8_t { Left = 0, Right = 1, Bottom = 2, Up = 3 };
enum class Corner : std::uint8_t { LB = 0, LU = 1, RU = 2, RB = 3 };

struct LeafCellKey {
	std::int32_t tree;
	std::int32_t level;
	std::int32_t x;
	std::int32_t y;
};

struct CanonicalNodeKey {
	std::uint64_t value;
};

struct CanonicalHangingFaceKey {
	std::uint64_t value;
};

struct MappingError {
	bool failed;
	const char *reason;
};

constexpr std::uint8_t kQuadCornerCount = 4;
constexpr std::uint8_t kQuadFaceCount = 4;
constexpr std::uint8_t kQuadEdgeOrderB[kQuadFaceCount] = {0, 1, 2, 3}; // L,R,B,U
constexpr std::uint8_t kQuadEdgeOrderC[kQuadFaceCount] = {0, 3, 1, 2}; // L,U,R,B
// p4est corner order LB,RB,LU,RU mapped to quad corner index.
constexpr std::uint8_t kP4estCornerToQuad[kQuadCornerCount] = {0, 3, 1, 2};

inline const char *face_name(Face face)
{
	switch (face) {
	case Face::Left: return "left";
	case Face::Right: return "right";
	case Face::Bottom: return "bottom";
	case Face::Up: return "up";
	}
	return "invalid";
}

inline const char *corner_name(Corner corner)
{
	switch (corner) {
	case Corner::LB: return "LB";
	case Corner::LU: return "LU";
	case Corner::RU: return "RU";
	case Corner::RB: return "RB";
	}
	return "invalid";
}

inline MappingError face_endpoints(Face face, std::uint8_t endpoints[2])
{
	switch (face) {
	case Face::Left: endpoints[0] = 0; endpoints[1] = 1; break;
	case Face::Right: endpoints[0] = 3; endpoints[1] = 2; break;
	case Face::Bottom: endpoints[0] = 0; endpoints[1] = 3; break;
	case Face::Up: endpoints[0] = 1; endpoints[1] = 2; break;
	default:
		return MappingError{true, "invalid face"};
	}
	return MappingError{false, nullptr};
}

inline MappingError fine_sibling_order(Face face, std::uint8_t fine_index,
	int &sibling_face, int &child_pair)
{
	// Two fine children share one coarse face.  fine_index 0 is the first
	// child in canonical coarse-edge order, fine_index 1 the second.
	if (fine_index > 1) {
		return MappingError{true, "invalid fine child index"};
	}
	switch (face) {
	case Face::Left:
	case Face::Right:
		sibling_face = static_cast<int>(face);
		child_pair = fine_index == 0 ? 1 : 0;
		break;
	case Face::Bottom:
	case Face::Up:
		sibling_face = static_cast<int>(face);
		child_pair = fine_index == 0 ? 3 : 2;
		break;
	default:
		return MappingError{true, "invalid face"};
	}
	return MappingError{false, nullptr};
}

inline std::uint8_t quad_corner_from_p4est(std::uint8_t p4est_corner)
{
	if (p4est_corner >= kQuadCornerCount) {
		return 0xFF;
	}
	return kP4estCornerToQuad[p4est_corner];
}

inline void reverse_endpoints(std::uint8_t endpoints[2])
{
	std::swap(endpoints[0], endpoints[1]);
}

inline bool operator==(const LeafCellKey &a, const LeafCellKey &b)
{
	return a.tree == b.tree && a.level == b.level &&
		a.x == b.x && a.y == b.y;
}

inline bool operator==(const CanonicalNodeKey &a, const CanonicalNodeKey &b)
{
	return a.value == b.value;
}

inline bool operator==(const CanonicalHangingFaceKey &a,
	const CanonicalHangingFaceKey &b)
{
	return a.value == b.value;
}

// Diagnostic node key from physical coordinates.  Cross-tree incident leaves
// at the same physical point must produce the same rounded key.
inline CanonicalNodeKey canonical_node_key_from_point(double x, double y)
{
	const std::uint64_t xi = static_cast<std::uint64_t>(x * 1048576.0 + 0.5);
	const std::uint64_t yi = static_cast<std::uint64_t>(y * 1048576.0 + 0.5);
	const std::uint64_t value = (xi << 32) ^ (yi & 0xFFFFFFFFu);
	return CanonicalNodeKey{value};
}

inline CanonicalNodeKey canonical_node_key(const LeafCellKey &cell,
	std::uint8_t corner, double x, double y)
{
	(void)cell;
	(void)corner;
	return canonical_node_key_from_point(x, y);
}

// Diagnostic hanging-face key from canonical endpoint keys.
inline CanonicalHangingFaceKey canonical_hanging_face_key(
	CanonicalNodeKey a, CanonicalNodeKey b, std::uint8_t segment)
{
	const std::uint64_t lo = a.value < b.value ? a.value : b.value;
	const std::uint64_t hi = a.value < b.value ? b.value : a.value;
	return CanonicalHangingFaceKey{((hi << 8) ^ lo) | (static_cast<std::uint64_t>(segment) & 0xFFu)};
}

} // namespace Nodal