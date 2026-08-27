#pragma once
#include "defines.h"
#include "variable.h"
#include "core/vector_matrix.h"
#include "alg.h"

// M14.6: pure coordinate-update kernel.
namespace HydroCallbacks {

inline void update_corner_coordinates(CVariable &vara, double delta_time)
{
	for (int k = 0; k < CNDIM; ++k) {
		const CDoubleVector &vel = vara.corner_vector(idcnVelocity_lag, k);
		vara.corner_vector(idcnCoords_lag, k) =
			vara.corner_vector(idcnCoords_half, k) +
			CDoubleVector(vel.x * delta_time, vel.y * delta_time);
	}

	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; ++i) {
		m_cell_coord[i] = vara.corner_vector(idcnCoords_lag, i);
	}
	vara.cell_vector(idCentroidCoord_lag) =
		GeometryAlg::GetPolyCenter(m_cell_coord);

	for (int idIndex = idEChildrenCoordinate_lag;
		idIndex < idVectorEdgeVariableNum; ++idIndex) {
		VectorCornerVariableID idcnVara;
		switch (idIndex) {
		case idEChildrenCoordinate_lag:
			idcnVara = idcnCoords_lag;
			break;
		case idEChildrenVelocity_lag:
			idcnVara = idcnVelocity_lag;
			break;
		default:
			idcnVara = idcnCoords_lag;
			break;
		}
		vara.edge_vector(static_cast<VectorEdgeVariableID>(idIndex),
			quad_data_t::EnumEdge::LEFT) = 0.5 *
			(vara.corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTUP) +
				vara.corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTBOTTOM));
		vara.edge_vector(static_cast<VectorEdgeVariableID>(idIndex),
			quad_data_t::EnumEdge::RIGHT) = 0.5 *
			(vara.corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTUP) +
				vara.corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTBOTTOM));
		vara.edge_vector(static_cast<VectorEdgeVariableID>(idIndex),
			quad_data_t::EnumEdge::BOTTOM) = 0.5 *
			(vara.corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTBOTTOM) +
				vara.corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTBOTTOM));
		vara.edge_vector(static_cast<VectorEdgeVariableID>(idIndex),
			quad_data_t::EnumEdge::UP) = 0.5 *
			(vara.corner_vector(idcnVara, quad_data_t::EnumCorner::LEFTUP) +
				vara.corner_vector(idcnVara, quad_data_t::EnumCorner::RIGHTUP));
	}
}

} // namespace HydroCallbacks
