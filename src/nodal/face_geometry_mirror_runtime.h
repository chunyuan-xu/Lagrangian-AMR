#pragma once

#include "defines.h"
#include "nodal/face_geometry_mirror.h"

// Runtime adapter that reads the legacy per-cell half-edge geometry and
// mirrors it into the inert CellNodalData.faces array for regular planar
// leaves.  It never changes legacy state or switches any reader.

namespace Nodal {

namespace detail {

struct LegacyFaceGeometrySample {
	double normal[2];
	double length;
	double rcp;
};

inline LegacyFaceGeometrySample sample_face_geometry(const quad_data_t &data,
	std::uint8_t corner, CHalf_edge_data::cside side)
{
	const CHalf_edge_data &half =
		data.m_cndata[corner].hdata[static_cast<int>(side)];
	LegacyFaceGeometrySample sample;
	sample.normal[0] = half.Ncp.x;
	sample.normal[1] = half.Ncp.y;
	sample.length = half.Lcp;
	sample.rcp = half.Rcp;
	return sample;
}

inline FaceGeometryMirrorError mirror_face_geometry_one(quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyFaceGeometrySample a = sample_face_geometry(data, endpoints[0], side_a);
	const LegacyFaceGeometrySample b = sample_face_geometry(data, endpoints[1], side_b);
	return build_regular_face(
		data.nodal.faces[static_cast<int>(face)], face, endpoints,
		a.normal, a.length, b.length, a.rcp, b.rcp);
}

inline FaceGeometryMirrorError verify_face_geometry_one(const quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyFaceGeometrySample a = sample_face_geometry(data, endpoints[0], side_a);
	const LegacyFaceGeometrySample b = sample_face_geometry(data, endpoints[1], side_b);
	return verify_regular_face(
		data.nodal.faces[static_cast<int>(face)], face, endpoints,
		a.normal, a.length, b.length, a.rcp, b.rcp);
}

inline FaceGeometryMirrorError mirror_hanging_face_one(quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyFaceGeometrySample a = sample_face_geometry(data, endpoints[0], side_a);
	const LegacyFaceGeometrySample b = sample_face_geometry(data, endpoints[1], side_b);
	return build_hanging_face(
		data.nodal.faces[static_cast<int>(face)], face, endpoints,
		a.normal, a.length, b.length, a.rcp, b.rcp);
}

inline FaceGeometryMirrorError verify_hanging_face_one(const quad_data_t &data, Face face,
	const std::uint8_t endpoints[2],
	CHalf_edge_data::cside side_a, CHalf_edge_data::cside side_b)
{
	const LegacyFaceGeometrySample a = sample_face_geometry(data, endpoints[0], side_a);
	const LegacyFaceGeometrySample b = sample_face_geometry(data, endpoints[1], side_b);
	return verify_hanging_face(
		data.nodal.faces[static_cast<int>(face)], face, endpoints,
		a.normal, a.length, b.length, a.rcp, b.rcp);
}

} // namespace detail

inline FaceGeometryMirrorError mirror_legacy_regular_geometry_to_faces(quad_data_t &data)
{
	std::uint8_t endpoints[2];
	MappingError merr;
	FaceGeometryMirrorError err;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::mirror_face_geometry_one(data, Face::Left, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::mirror_face_geometry_one(data, Face::Right, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::mirror_face_geometry_one(data, Face::Bottom, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::mirror_face_geometry_one(data, Face::Up, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	return FaceGeometryMirrorError{false, nullptr};
}

inline FaceGeometryMirrorError mirror_legacy_geometry_to_faces(quad_data_t &data)
{
	std::uint8_t endpoints[2];
	MappingError merr;
	FaceGeometryMirrorError err;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Left)].IsParentChildBoun) {
		err = detail::mirror_hanging_face_one(data, Face::Left, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
		if (err.failed) { return err; }
	} else {
		err = detail::mirror_face_geometry_one(data, Face::Left, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
		if (err.failed) { return err; }
	}

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Right)].IsParentChildBoun) {
		err = detail::mirror_hanging_face_one(data, Face::Right, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
		if (err.failed) { return err; }
	} else {
		err = detail::mirror_face_geometry_one(data, Face::Right, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
		if (err.failed) { return err; }
	}

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Bottom)].IsParentChildBoun) {
		err = detail::mirror_hanging_face_one(data, Face::Bottom, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
		if (err.failed) { return err; }
	} else {
		err = detail::mirror_face_geometry_one(data, Face::Bottom, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
		if (err.failed) { return err; }
	}

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Up)].IsParentChildBoun) {
		err = detail::mirror_hanging_face_one(data, Face::Up, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
		if (err.failed) { return err; }
	} else {
		err = detail::mirror_face_geometry_one(data, Face::Up, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
		if (err.failed) { return err; }
	}

	return FaceGeometryMirrorError{false, nullptr};
}

inline FaceGeometryMirrorError verify_legacy_geometry_to_faces(const quad_data_t &data)
{
	std::uint8_t endpoints[2];
	MappingError merr;
	FaceGeometryMirrorError err;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Left)].IsParentChildBoun) {
		err = detail::verify_hanging_face_one(data, Face::Left, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	} else {
		err = detail::verify_face_geometry_one(data, Face::Left, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	}
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Right)].IsParentChildBoun) {
		err = detail::verify_hanging_face_one(data, Face::Right, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	} else {
		err = detail::verify_face_geometry_one(data, Face::Right, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	}
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Bottom)].IsParentChildBoun) {
		err = detail::verify_hanging_face_one(data, Face::Bottom, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	} else {
		err = detail::verify_face_geometry_one(data, Face::Bottom, endpoints,
			CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	}
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	if (data.m_pc_edge_data[static_cast<int>(Face::Up)].IsParentChildBoun) {
		err = detail::verify_hanging_face_one(data, Face::Up, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	} else {
		err = detail::verify_face_geometry_one(data, Face::Up, endpoints,
			CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	}
	if (err.failed) { return err; }

	return verify_cell_closure(data.nodal.faces);
}

inline FaceGeometryMirrorError verify_legacy_regular_geometry_to_faces(
	const quad_data_t &data)
{
	std::uint8_t endpoints[2];
	MappingError merr;
	FaceGeometryMirrorError err;

	merr = face_endpoints(Face::Left, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::verify_face_geometry_one(data, Face::Left, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Right, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::verify_face_geometry_one(data, Face::Right, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Bottom, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::verify_face_geometry_one(data, Face::Bottom, endpoints,
		CHalf_edge_data::cside::minus, CHalf_edge_data::cside::plus);
	if (err.failed) { return err; }

	merr = face_endpoints(Face::Up, endpoints);
	if (merr.failed) { return FaceGeometryMirrorError{true, merr.reason}; }
	err = detail::verify_face_geometry_one(data, Face::Up, endpoints,
		CHalf_edge_data::cside::plus, CHalf_edge_data::cside::minus);
	if (err.failed) { return err; }

	return verify_regular_cell_closure(data.nodal.faces);
}

} // namespace Nodal
