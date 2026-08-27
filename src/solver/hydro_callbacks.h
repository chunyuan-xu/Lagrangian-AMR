#pragma once
#include <cmath>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include "amr/parent_edge_view.h"
#include "hydro/divergence_kernel.h"
#include "hydro/volume_density_kernel.h"
#include "physics/eos.h"
#include "physics/physics_alg.h"

// M7.5: HydroPhases — per-cell hydro update callbacks. These are the concrete
// numerical field-update formulas (density, momentum, work, energy, EOS,
// sound speed, divergence). They are pure per-cell functions over the
// quadrant data; the forest traversal wrapper lives in HydroPhases::run_volume_update.

namespace HydroPhases {


void quadrant_compute_divergence_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	
	
	CDoubleVector	cnVelocity[CNDIM];
	CDoubleVector	cnCoord[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		cnCoord[k] = m_vara->corner_vector(idcnCoords_lag, k);
		cnVelocity[k] = m_vara->corner_vector(idcnVelocity_lag, k);
	}
	m_vara->cell(idDivergence) = HydroCallbacks::compute_divergence(
		p4est_data->coord_type, cnCoord, cnVelocity);
}

void quadrant_compute_soundspeed_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idSoundSpeed) = PhysicalAlg::CalculateSoundSpeed(
		m_vara->cell(idGamma),
		m_vara->cell(idPressure_lag),
		m_vara->cell(idDensity_lag));
}

void quadrant_update_density_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	HydroCallbacks::update_volume_density(*m_vara, coordinate_type);
}

void quadrant_update_momentum_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	AMRCallbacks::ParentEdgeView	parent_edges(*data);
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	int					scheme_type = p4est_data->Scheme_type;
	CDoubleVector		SumFcp = CDoubleVector(0., 0.);
	CDoubleVector		center_point;
	double				m_alpha = 0.;
	if (coordinate_type == p4est_data_t::MyCoordType::cylinder
		&& scheme_type == p4est_data_t::MySchemeType::ControlVolume) {
		m_alpha = 1.;
	}
	CDoubleVector m_baser = CDoubleVector(1., 0.);
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		if (scheme_type == p4est_data_t::MySchemeType::ControlVolume) 
		{
			SumFcp += m_vara->corner_vector(idcnFcp, cnid) + m_vara->corner_vector(idcnFluxRelaxed, cnid);
		}
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
		{
			if (parent_edges.at(eind).IsParentChildBoun==true)
			{
				SumFcp += m_vara->corner_vector(ideFcp, eind) + parent_edges.at(eind).FluxRelaxed;
			}
		}
	}

	if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
	{
		m_vara->cell_vector(idCentroidVelo_lag) = m_vara->cell_vector(idCentroidVelo_half) -
			p4est_data->dt_iter * SumFcp / m_vara->cell(idMass);
	}
	else if (scheme_type == p4est_data_t::MySchemeType::AreaWeighted)
	{
		CDoubleVector m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->corner_vector(idcnCoords_cur, i); }
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		m_vara->cell_vector(idCentroidVelo_lag) = m_vara->cell_vector(idCentroidVelo_half) -
			p4est_data->dt_iter * SumFcp / m_vara->cell(idMass) / center_point.x;
	}
}

void quadrant_compute_work_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;
	int					coordinate_type = p4est_data->coord_type;
	double				m_alpha = 1.;
	double				m_beta = 1.;

	
	m_vara->cell(idKineticVariation) = 0.;
	m_vara->cell(idTotalWork) = 0.;

	if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
	{
		m_alpha = 2.* M_PI;
		m_beta = 2. * M_PI * m_vara->cell_vector(idCentroidCoord_cur).y;
	}
	CDoubleVector Velo = 0.5 * (m_vara->cell_vector(idCentroidVelo_half) + m_vara->cell_vector(idCentroidVelo_lag));
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		
		if (coordinate_type == p4est_data_t::MyCoordType::plane)
		{
			
			m_vara->cell(idKineticVariation) += m_beta * 
				Velo^ (m_vara->corner_vector(idcnFcp, cnid)+ m_vara->corner_vector(idcnFluxRelaxed, cnid));
		}
		if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
		{
			
			m_vara->cell(idKineticVariation) += m_beta * Velo^ m_vara->corner_vector(idAWFcp, cnid);
		}

		
		m_vara->cell(idTotalWork) += m_alpha*
			m_vara->corner_vector(idcnVelocity_lag, cnid) ^ 
			(m_vara->corner_vector(idcnFcp, cnid)+ m_vara->corner_vector(idcnFluxRelaxed, cnid)); 
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		
		if (PCInfo[eind].IsParentChildBoun==true)
		{
			if (coordinate_type == p4est_data_t::MyCoordType::plane)
			{
				m_vara->cell(idKineticVariation) += m_beta * Velo ^
					(m_vara->corner_vector(ideFcp, eind) + PCInfo[eind].FluxRelaxed);
			}
			m_vara->cell(idTotalWork) += m_alpha* PCInfo[eind].Hanging_velocity ^
				(m_vara->corner_vector(ideFcp, eind) + PCInfo[eind].FluxRelaxed);
		}
	}
}

void quadrant_update_energy_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = &((P4estBridge *)info->p4est->user_pointer)->data;

	
	m_vara->cell(idTotalEnergy_lag) = m_vara->cell(idTotalEnergy_half) - p4est_data->dt_iter * m_vara->cell(idTotalWork) / m_vara->cell(idMass);


	double source = 0.;
	if (p4est_data->which_case == ProblemNo::TaylorGreen)
	{
		source = p4est_data->dt_iter * 5.*M_PI / 8.*m_vara->cell(idVolume) *
			(cos(3.*M_PI*m_vara->cell_vector(idCentroidCoord_lag).x)*cos(M_PI * m_vara->cell_vector(idCentroidCoord_lag).y) -
				cos(M_PI*m_vara->cell_vector(idCentroidCoord_lag).x)*cos(3.*M_PI*m_vara->cell_vector(idCentroidCoord_lag).y)) / m_vara->cell(idMass);
	}

	if (m_vara->cell(idTotalEnergy_lag) > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}

	
	m_vara->cell(idInternalEnergy_lag) = m_vara->cell(idInternalEnergy_half) - p4est_data->dt_iter
		* (m_vara->cell(idTotalWork) - m_vara->cell(idKineticVariation)) / m_vara->cell(idMass);
	m_vara->cell(idInternalEnergy_lag) += source;
	if (m_vara->cell(idInternalEnergy_lag) > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}
}

void quadrant_update_EOS_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	m_vara->cell(idPressure_lag) = PhysicalAlg::EquationOfState(m_vara->cell(idGamma), m_vara->cell(idDensity_lag), m_vara->cell(idInternalEnergy_lag));
	if (m_vara->cell(idPressure_lag) > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the value of pressure is illegal\n");
	}
}

} // namespace HydroPhases
