#pragma once

#include "defines.h"
#include "nodal/boundary_mirror.h"

// Runtime adapter that reads the legacy per-cell half-edge boundary
// classification and mirrors it into the inert CellNodalData.boundaries
// array.  It never changes legacy state or switches any reader.

namespace Nodal {

namespace detail {

struct LegacyBoundarySample {
	int type;
	double value;
	double normal[2];
	double length;
};

inline LegacyBoundarySample sample_face_corner(const quad_data_t &data,
	std::uint8_t corner, CHalf_edge_data::cside side)
{
	const CHalf_edge_data &half =
		data.m_cndata[corner].hdata[static_cast<int>(side)];
	LegacyBoundarySample sample;
	sample.type = half.enumBYD;
	sample.value = half.BYDVal;
	sample.normal[0] = half.Ncp.x;
	sample.normal[1] = half.Ncp.y;
	sample.length = half.Lcp;
	return sample;
}

inline BoundaryMirrorError mirror_one_face(quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyBoundarySample a = sample_face_corner(data, endpoints[0], side_a);
	const LegacyBoundarySample b = sample_face_corner(data, endpoints[1], side_b);
	return build_face_boundary(
		data.nodal.boundaries[static_cast<int>(face)], face, endpoints,
		a.type, a.value, a.normal, a.length,
		b.type, b.value, b.normal, b.length);
}

inline BoundaryMirrorError verify_one_face(const quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyBoundarySample a = sample_face_corner(data, endpoints[0], side_a);
	const LegacyBoundarySample b = sample_face_corner(data, endpoints[1], side_b);
	return verify_face_boundary(
		data.nodal.boundaries[static_cast<int>(face)], face, endpoints,
		a.type, a.value, a.normal, a.length,
		b.type, b.value, b.normal, b.length);
}

} // namespace detail

inline BoundaryMirrorError mirror_legacy_boundary_to_faces(quad_data_t &data)
{
	std::uint8_t endpoints[2];
	BoundaryMirrorError err;
	MappingError merr;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::mirror_one_face(data, Face::Left, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::mirror_one_face(data, Face::Right, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::mirror_one_face(data, Face::Bottom, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::mirror_one_face(data, Face::Up, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	return BoundaryMirrorError{false, nullptr};
}

inline BoundaryMirrorError verify_legacy_boundary_to_faces(const quad_data_t &data)
{
	std::uint8_t endpoints[2];
	BoundaryMirrorError err;
	MappingError merr;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::verify_one_face(data, Face::Left, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::verify_one_face(data, Face::Right, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::verify_one_face(data, Face::Bottom, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return BoundaryMirrorError{true, merr.reason}; }
	err = detail::verify_one_face(data, Face::Up, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	return BoundaryMirrorError{false, nullptr};
}

} // namespace Nodal
