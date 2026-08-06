#pragma once
#include <cmath>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "physics/eos.h"

// M4.2: AMRTransfer — pure transfer interface extracted from the p4est
// replace callback. Coarsen aggregates four children into a parent;
// refine distributes a parent into four children. Field order and all
// numeric formulas are byte-identical to the original Lagrangian_replace_quads.

namespace AMRTransfer {

inline void coarsen_children_to_parent(p4est_data_t *p4est_data,
	quad_data_t *parent_data,
	quad_data_t *child_data1, quad_data_t *child_data2,
	quad_data_t *child_data3, quad_data_t *child_data4)
{
	enum m_geometry_id {m_coord, m_velo};
	enum m_physical_id {m_density, m_internal_energy};
	enum m_which_child {child1, child2, child3, child4};

	parent_data->m_vara.int_cell(idCoarseningTag) = p4est_data_t::CoarseningEnum::CoarsenedJustNow;

	for (int idIndex = idcnCoords_cur; idIndex <= idcnVelocity_lag; idIndex++)
	{
		parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTBOTTOM) =
			child_data1->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTBOTTOM);
		parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTUP) =
			child_data3->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::LEFTUP);
		parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTUP) =
			child_data4->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTUP);
		parent_data->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTBOTTOM) =
			child_data2->m_vara.corner_vector(static_cast<VectorCornerVariableID>(idIndex), quad_data_t::EnumCorner::RIGHTBOTTOM);
	}

	for (int idIndex = m_geometry_id::m_coord; idIndex <= m_geometry_id::m_velo; idIndex++)
	{
		VectorCornerVariableID idChildIndex;
		switch (idIndex)
		{
		case m_geometry_id::m_coord:
			idChildIndex = idcnCoords_lag;
			break;
		case m_geometry_id::m_velo:
			idChildIndex = idcnVelocity_lag;
			break;
		default:
			break;
		}

		for (int cnid = 0; cnid < CNDIM; cnid++)
		{
			parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child1][cnid] =
				child_data1->m_vara.corner_vector(idChildIndex, cnid);
		}
		for (int cnid = 0; cnid < CNDIM; cnid++)
		{
			parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child2][cnid] =
				child_data2->m_vara.corner_vector(idChildIndex, cnid);
		}
		for (int cnid = 0; cnid < CNDIM; cnid++)
		{
			parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child3][cnid] =
				child_data3->m_vara.corner_vector(idChildIndex, cnid);
		}
		for (int cnid = 0; cnid < CNDIM; cnid++)
		{
			parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child4][cnid] =
				child_data4->m_vara.corner_vector(idChildIndex, cnid);
		}
	}

	for (int idIndex = m_physical_id::m_density; idIndex <= m_physical_id::m_internal_energy; idIndex++)
	{
		DoubleCellVariableID idChildIndex;
		switch (idIndex)
		{
		case m_physical_id::m_density:
			idChildIndex = idDensity_lag;
			break;
		case m_physical_id::m_internal_energy:
			idChildIndex = idInternalEnergy_lag;
			break;
		default:
			break;
		}

		parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child1] =
			child_data1->m_vara.cell(idChildIndex);
		parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child2] =
			child_data2->m_vara.cell(idChildIndex);
		parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child3] =
			child_data3->m_vara.cell(idChildIndex);
		parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child4] =
			child_data4->m_vara.cell(idChildIndex);
		if (parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child1] > m_eps &&
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child2] > m_eps &&
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child3] > m_eps &&
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child4] > m_eps)
		{
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("The value of ChildrenPhysicalVara is illegal in refining!\n");
			abort();
		}
	}

	parent_data->m_vara.cell(idMass) = child_data1->m_vara.cell(idMass) +
		child_data2->m_vara.cell(idMass) +
		child_data3->m_vara.cell(idMass) +
		child_data4->m_vara.cell(idMass);

	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = parent_data->m_vara.corner_vector(idcnCoords_cur, i); }
	parent_data->m_vara.cell(idVolume) = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
	parent_data->m_vara.cell(idDensity_cur) = parent_data->m_vara.cell(idMass) / parent_data->m_vara.cell(idVolume);
	parent_data->m_vara.cell(idDensity_lag) = parent_data->m_vara.cell(idDensity_cur);
	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	parent_data->m_vara.cell_vector(idCentroidCoord_cur) = center_point;

	parent_data->m_vara.cell_vector(idCentroidVelo_cur) = (child_data1->m_vara.cell(idMass) * child_data1->m_vara.cell_vector(idCentroidVelo_cur) +
		child_data2->m_vara.cell(idMass) * child_data2->m_vara.cell_vector(idCentroidVelo_cur) +
		child_data3->m_vara.cell(idMass) * child_data3->m_vara.cell_vector(idCentroidVelo_cur) +
		child_data4->m_vara.cell(idMass) * child_data4->m_vara.cell_vector(idCentroidVelo_cur))
		/ parent_data->m_vara.cell(idMass);
	parent_data->m_vara.cell_vector(idCentroidVelo_lag) = parent_data->m_vara.cell_vector(idCentroidVelo_cur);

	parent_data->m_vara.cell(idGamma) = (
		child_data1->m_vara.cell(idGamma) +
		child_data2->m_vara.cell(idGamma) +
		child_data3->m_vara.cell(idGamma) +
		child_data4->m_vara.cell(idGamma)) / P4EST_CHILDREN;

	parent_data->m_vara.cell(idTotalEnergy_cur) = (
		child_data1->m_vara.cell(idMass) * child_data1->m_vara.cell(idTotalEnergy_cur) +
		child_data2->m_vara.cell(idMass) * child_data2->m_vara.cell(idTotalEnergy_cur) +
		child_data3->m_vara.cell(idMass) * child_data3->m_vara.cell(idTotalEnergy_cur) +
		child_data4->m_vara.cell(idMass) * child_data4->m_vara.cell(idTotalEnergy_cur))
		/ parent_data->m_vara.cell(idMass);
	parent_data->m_vara.cell(idTotalEnergy_lag) = parent_data->m_vara.cell(idTotalEnergy_cur);

	parent_data->m_vara.cell(idInternalEnergy_cur) = parent_data->m_vara.cell(idTotalEnergy_cur) -
		0.5 * (pow(parent_data->m_vara.cell_vector(idCentroidVelo_cur).x, 2) + pow(parent_data->m_vara.cell_vector(idCentroidVelo_cur).y, 2));
	if (parent_data->m_vara.cell(idInternalEnergy_cur) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The value of internal energy is illegal in refining!\n");
		abort();
	}
	parent_data->m_vara.cell(idInternalEnergy_lag) = parent_data->m_vara.cell(idTotalEnergy_lag) -
		0.5 * (pow(parent_data->m_vara.cell_vector(idCentroidVelo_lag).x, 2) + pow(parent_data->m_vara.cell_vector(idCentroidVelo_lag).y, 2));
	parent_data->m_vara.cell(idInternalEnergy_lag) = parent_data->m_vara.cell(idInternalEnergy_cur);

	parent_data->m_vara.cell(idPressure_lag) = PhysicalAlg::EquationOfState(
		parent_data->m_vara.cell(idGamma),
		parent_data->m_vara.cell(idDensity_lag),
		parent_data->m_vara.cell(idInternalEnergy_lag));
	parent_data->m_vara.cell(idPressure_cur) = parent_data->m_vara.cell(idPressure_lag);
	parent_data->m_vara.cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
		parent_data->m_vara.cell(idGamma),
		parent_data->m_vara.cell(idPressure_cur),
		parent_data->m_vara.cell(idDensity_cur));
}

} // namespace AMRTransfer
