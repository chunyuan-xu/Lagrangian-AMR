#pragma once
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "hydro/hydro_callbacks.h"
#include "solver/hydro_callbacks.h"

// M9.3: Initializer — mesh and physical-field initialization callbacks
// stripped from main.cpp. Lagrangian_init_condition is the p4est init
// callback signature passed to p4est_new_ext / p4est_balance.

namespace Initializer {


void get_boundary_from_p4est(p4est_t *p4est)
{
	p4est_data_t *p4est_data = &((P4estBridge *)p4est->user_pointer)->data;

	PhysicalAlg::InitBoundaryCondition(p4est_data->which_case,
		p4est_data->coord_type,
		p4est_data->TopBoun,
		p4est_data->BottomBoun,
		p4est_data->LeftBoun,
		p4est_data->RightBoun,
		p4est_data->TopBounVal,
		p4est_data->BottomBounVal,
		p4est_data->LeftBounVal,
		p4est_data->RightBounVal);

	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		HydroCallbacks::quadrant_get_BYD_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

void Lagrangian_init_condition(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{

	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	p4est_connectivity_t *connectivity = p4est->connectivity;
	// M10.4.1: user_pointer is a P4estBridge carrier; unpack the payload.
	p4est_data_t			*p4est_data = &((P4estBridge *)p4est->user_pointer)->data;

	// M10.2.1: coord_type / Scheme_type are read-only config set by the
	// p4est_data_t constructor (both default to plane/ControlVolume). The
	// per-callback rewrites were redundant writes into config; removed so
	// the init callback does not mutate frozen configuration.

	int			level = q->level;

	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	
	double dx = 1.0 / (1 << level);


	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;

	int index_i = int(qx / length);
	int index_j = int(qy / length);
	int width_num = (1 << level);

	
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy, data->init_node_coords[0]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy + length, data->init_node_coords[1]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy + length, data->init_node_coords[2]);
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy, data->init_node_coords[3]);

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->corner_vector(idcnCoords_cur, i).x = data->init_node_coords[i][0];
		m_vara->corner_vector(idcnCoords_cur, i).y = data->init_node_coords[i][1];
		m_vara->corner_vector(idcnVelocity_cur, i) = CDoubleVector(0.0, 0.0);
		m_vara->corner_vector(idcnVelocity_lag, i) = CDoubleVector(0.0, 0.0);
	}

	CDoubleVector cnCoordCur[CNDIM], cnCoordLag[CNDIM], cnVeloCur[CNDIM], cnVeloLag[CNDIM];
	for (int i = 0; i < CNDIM; i++)
	{
		cnCoordCur[i] = m_vara->corner_vector(idcnCoords_cur, i);
		cnCoordLag[i] = m_vara->corner_vector(idcnCoords_lag, i);
		cnVeloCur[i] = m_vara->corner_vector(idcnVelocity_cur, i);
		cnVeloLag[i] = m_vara->corner_vector(idcnVelocity_lag, i);
	}

	PhysicalAlg::InitCondition(p4est_data->which_case,
		p4est_data->coord_type, int(qx), int(qy), index_i, index_j, width_num,
		cnCoordCur, cnCoordLag, cnVeloCur, cnVeloLag,
		m_vara->cell(idDensity_cur),
		m_vara->cell(idDensity_lag),
		m_vara->cell(idVolume),
		m_vara->cell(idMass),
		m_vara->cell_vector(idCentroidCoord_cur),
		m_vara->cell_vector(idCentroidCoord_lag),
		m_vara->cell_vector(idCentroidVelo_cur),
		m_vara->cell_vector(idCentroidVelo_lag),
		m_vara->cell(idInternalEnergy_cur),
		m_vara->cell(idInternalEnergy_lag),
		m_vara->cell(idPressure_cur),
		m_vara->cell(idPressure_lag),
		m_vara->cell(idTotalEnergy_cur),
		m_vara->cell(idTotalEnergy_lag),
		m_vara->cell(idSoundSpeed),
		m_vara->cell(idGamma),
		p4est_data->TopBoun,
		p4est_data->BottomBoun,
		p4est_data->LeftBoun,
		p4est_data->RightBoun,
		p4est_data->TopBounVal,
		p4est_data->BottomBounVal,
		p4est_data->LeftBounVal,
		p4est_data->RightBounVal);

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->corner_vector(idcnCoords_cur, i) = cnCoordCur[i];
		m_vara->corner_vector(idcnCoords_lag, i) = cnCoordLag[i];
		m_vara->corner_vector(idcnVelocity_cur, i) = cnVeloCur[i];
		m_vara->corner_vector(idcnVelocity_lag, i) = cnVeloLag[i];
	}

	HydroCallbacks::generate_children_info_from_parent(p4est_data, m_vara);
}

} // namespace Initializer
