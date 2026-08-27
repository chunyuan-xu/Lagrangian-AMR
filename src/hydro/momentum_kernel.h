#pragma once
#include "defines.h"
#include "variable.h"
#include "amr/parent_edge_view.h"
#include "alg.h"

// M14.8: pure momentum-update kernel.
namespace HydroCallbacks {

inline void update_momentum(
	CVariable &vara,
	AMRCallbacks::ParentEdgeView &parent_edges,
	int coordinate_type,
	int scheme_type,
	double dt_iter)
{
	CDoubleVector	SumFcp = CDoubleVector(0., 0.);
	CDoubleVector	center_point;

	if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
	{
		for (int cnid = 0; cnid < CNDIM; cnid++)
		{
			SumFcp += vara.corner_vector(idcnFcp, cnid)
				+ vara.corner_vector(idcnFluxRelaxed, cnid);
		}
		for (int eind = 0; eind < CNDIM; eind++)
		{
			if (parent_edges.at(eind).IsParentChildBoun == true)
			{
				SumFcp += vara.corner_vector(ideFcp, eind)
					+ parent_edges.at(eind).FluxRelaxed;
			}
		}

		vara.cell_vector(idCentroidVelo_lag) =
			vara.cell_vector(idCentroidVelo_half)
			- dt_iter * SumFcp / vara.cell(idMass);
	}
	else if (scheme_type == p4est_data_t::MySchemeType::AreaWeighted)
	{
		CDoubleVector	m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++)
		{
			m_cell_coord[i] = vara.corner_vector(idcnCoords_cur, i);
		}
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		vara.cell_vector(idCentroidVelo_lag) =
			vara.cell_vector(idCentroidVelo_half)
			- dt_iter * SumFcp / vara.cell(idMass) / center_point.x;
	}
}

} // namespace HydroCallbacks
