#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

// T1: Scalar POD storage types for the DBGF nodal solver.
// These types are intentionally NOT included from defines.h and are not
// embedded in quad_data_t yet.  They exist to freeze layout and reset
// semantics before any runtime writer/reader is introduced.
//
// All wire-visible flags are uint8_t, never bool.

namespace Nodal {

enum class NodalRole : std::uint8_t {
	Invalid = 0,
	Local = 1,
	Master = 2,
	HangingRole = 3
};

enum class StagePhase : std::uint8_t {
	Invalid = 0,
	Assemble = 1,
	Solve = 2,
	Commit = 3
};

enum class ValidityFlag : std::uint8_t {
	Invalid = 0,
	Valid = 1
};

struct Vec2Storage {
	double x;
	double y;
};

struct Mat2Storage {
	double m[2][2];
};

struct StageStamp {
	std::uint64_t generation;
	std::uint64_t topology_version;
	std::uint32_t step;
	std::uint16_t sub_stage;
	std::uint8_t phase;
	std::uint8_t validity;
};

struct EdgeSegmentGeometry {
	Vec2Storage normal;
	double length;
	double endpoint_weights[2];
};

struct FaceData {
	std::uint8_t flags;
	std::uint8_t reserved[3];
	std::uint32_t logical_header;
	EdgeSegmentGeometry segments[2];
};

struct CellMasterContribution {
	double M[4][4];
	double b[8];
};

struct CellHangingContribution {
	double M[4][4];
	double b[8];
};

// Stack-only aggregated hanging contribution; distinct and not convertible.
struct AggregatedHangingContribution {
	double M[4][4];
	double b[8];
};

// Endpoint-ledger condensed contribution; distinct and not convertible.
struct CondensedMasterContribution {
	double M[4][4];
	double b[8];
};

struct MasterSolveState {
	Vec2Storage velocities[4];
};

struct EvaluatedCellFlux {
	double values[20];
};

struct CellNodalData {
	StageStamp stage;
	FaceData faces[4];
	CellMasterContribution master;
	CellHangingContribution hanging;
	CondensedMasterContribution condensed;
	MasterSolveState solved;
	EvaluatedCellFlux evaluated;
};

inline void reset_storage(StageStamp &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(Vec2Storage &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(Mat2Storage &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(EdgeSegmentGeometry &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(FaceData &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(CellMasterContribution &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(CellHangingContribution &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(AggregatedHangingContribution &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(CondensedMasterContribution &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(MasterSolveState &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(EvaluatedCellFlux &value)
{
	std::memset(&value, 0, sizeof(value));
}

inline void reset_storage(CellNodalData &value)
{
	std::memset(&value, 0, sizeof(value));
}

// Compile-time evidence that all storage records are raw-byte eligible.
template <typename T>
struct RawByteEligible : std::integral_constant<bool,
	std::is_standard_layout<T>::value &&
	std::is_trivially_copyable<T>::value &&
	std::is_trivially_destructible<T>::value> {};

} // namespace Nodal