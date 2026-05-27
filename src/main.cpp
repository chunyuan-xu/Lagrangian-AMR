#include "alg.h"
#ifdef _WIN32
#include <direct.h>
#else
#include<sys/stat.h>
#include<sys/types.h>
#endif // _WIN32
using namespace std;

#ifndef P4_TO_P8
#include<p4est_vtk.h>
#include<p4est_bits.h>
#include<p4est_extended.h>
#include<p4est_iterate.h>
#include<p4est_io.h>
#include<p4est_communication.h>
#include<windows.h>
#else
#include<p8est_vtk.h>
#include<p8est_bits.h>
#include<p8est_extended.h>
#include<p8est_iterate.h>
#include<p8est_io.h>
#include<p8est_communication.h>
#endif // !P4_TO_P8

































































/*根据逻辑坐标，确定悬挂边的位置*/
static void get_hanging_edge_info_from_logical_position(const int which_face, const p4est_qcoord_t qx1, const p4est_qcoord_t qy1,
	const p4est_qcoord_t qx2, const p4est_qcoord_t qy2, const p4est_qcoord_t length,
	int which_corner[2], int which_side[2], int master_corner[2], int unconstrained_master_corner[2])
{
	switch (which_face)
	{
	case quad_data_t::EnumEdge::LEFT:/*左边界*/
		if (qy1 == qy2 + length)
		{
			/*quad的左边界下顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::plus;

			/*quad_aside的左边界上顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else if (qy1 + length == qy2)
		{
			/*quad的左边界上顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[0] = CHalf_edge_data::cside::minus;

			/*quad_aside的左边界下顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::RIGHT:/*右边界*/
		if (qy1 == qy2 + length)
		{
			/*quad的右边界下顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::minus;

			/*quad_aside的右边界上顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else if (qy1 + length == qy2)
		{
			/*quad的右边界上顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			which_side[0] = CHalf_edge_data::cside::plus;

			/*quad_aside的右边界下顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::BOTTOM:/*下边界*/
		if (qx1 == qx2 + length)
		{
			/*quad的下边界左顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			which_side[0] = CHalf_edge_data::cside::minus;

			/*quad_aside的下边界右顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else if (qx1 + length == qx2)
		{
			/*quad的下边界右顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			which_side[0] = CHalf_edge_data::cside::plus;

			/*quad_aside的下边界左顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	case quad_data_t::EnumEdge::UP:/*上边界*/
		if (qx1 == qx2 + length)
		{
			/*quad的上边界左顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::plus;

			/*quad_aside的上边界右顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::minus;
		}
		else if (qx1 + length == qx2)
		{
			/*quad的上边界右顶点为悬点*/
			which_corner[0] = quad_data_t::EnumCorner::RIGHTUP;
			master_corner[0] = quad_data_t::EnumCorner::LEFTUP;
			unconstrained_master_corner[0] = quad_data_t::EnumCorner::RIGHTBOTTOM;
			which_side[0] = CHalf_edge_data::cside::minus;

			/*quad_aside的上边界左顶点为悬点*/
			which_corner[1] = quad_data_t::EnumCorner::LEFTUP;
			master_corner[1] = quad_data_t::EnumCorner::RIGHTUP;
			unconstrained_master_corner[1] = quad_data_t::EnumCorner::LEFTBOTTOM;
			which_side[1] = CHalf_edge_data::cside::plus;
		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("wrong in get_hanging_edge_info_from_logical_position!");
		}
		break;
	default:
		P4EST_GLOBAL_PRODUCTIONF("the value of which_face must be between 0 and 3!");
		break;
	}
}

/*定义基本几何量回调函数*/
static void quadrant_compute_RcpLcpNcp_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	/*1.获取当前quadrant的用户指针*/
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				SolverType = p4est_data->solver_type;
	CHalf_edge_data	*m_plus, *m_minus;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;

	/*计算边中点*/
	CDoubleVector EdgeMiddle[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		EdgeMiddle[k] = 0.5 * (m_vara->VecCnData[idcnCoords_cur][k] + m_vara->VecCnData[idcnCoords_cur][knext]);
	}
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		int kpre = GeometryAlg::GetCirclePre(CNDIM, k);
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		if (p4est_data->coord_type == p4est_data_t::MyCoordType::plane)
		{
			m_plus->Rcp = 1.0;
			m_minus->Rcp = 1.0;
		}
		else
		{
			if (SolverType == p4est_data_t::RiemannSolver::GridAligned)
			{
				m_plus->Rcp = GeometryAlg::GetRcpWeightWithLinearDistribution(m_vara->VecCnData[idcnCoords_cur][k],EdgeMiddle[k]);
				m_minus->Rcp = GeometryAlg::GetRcpWeightWithLinearDistribution(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[kpre]);
			}
			if (SolverType == p4est_data_t::RiemannSolver::Rotated)
			{
				m_plus->Rcp = GeometryAlg::GetRcpWeightWithConstDistribution(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[k]);
				m_minus->Rcp = GeometryAlg::GetRcpWeightWithConstDistribution(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[kpre]);
			}
		}

		m_plus->Lcp = GeometryAlg::GetPointToPointDistance(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[k]);
		m_minus->Lcp = GeometryAlg::GetPointToPointDistance(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[kpre]);

		/*NcpPlus计算*/
		m_plus->Ncp = GeometryAlg::GetLineNormalVector(m_vara->VecCnData[idcnCoords_cur][k], EdgeMiddle[k]);

		/*NcpMinus计算*/
		m_minus->Ncp = GeometryAlg::GetLineNormalVector(EdgeMiddle[kpre], m_vara->VecCnData[idcnCoords_cur][k]);
	}
}

/*松弛算法所需要的基本几何信息回调函数*/
static void quadrant_compute_relaxed_info_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	/*1.获取当前quadrant的用户指针*/
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				SolverType = p4est_data->solver_type;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;

	double			delta_time = p4est_data->dt_iter;
	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->VecCnData[idcnCoords_relaxed][k] = m_vara->VecCnData[idcnCoords_half][k] +
			CDoubleVector(m_vara->VecCnData[idcnVelocity_relaxed][k].x * delta_time, m_vara->VecCnData[idcnVelocity_relaxed][k].y * delta_time);
	}
}











































static void quadrant_relaxed_hanging_solver_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];
	/*two sides of the interface*/
	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				/*|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]*/) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;
			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector	master_velocity[2];//主点a和b的速度
			switch (parent_face_index)
			{
				case quad_data_t::EnumEdge::LEFT:
					master_velocity[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
					master_velocity[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
					break;
				case quad_data_t::EnumEdge::RIGHT:
					master_velocity[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
					master_velocity[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
					break;
				case quad_data_t::EnumEdge::BOTTOM:
					master_velocity[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
					master_velocity[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
					break;
				case quad_data_t::EnumEdge::UP:
					master_velocity[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
					master_velocity[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
					break;
			}

			CDoubleVector hanging_velo = 0.5 * (master_velocity[0] + master_velocity[1]);

			m_child1_data->m_vara.VecCnData[idcnVelocity_lag][m_which_corner[0]] = hanging_velo;
			m_child2_data->m_vara.VecCnData[idcnVelocity_lag][m_which_corner[1]] = hanging_velo;

			CDoubleMatrix MatrixP = m_child1_data->points[m_which_corner[0]].MatrixP;
			CDoubleVector m_rhs = m_child1_data->points[m_which_corner[0]].RHS;
			CDoubleVector Flux_relaxed;
			Flux_relaxed.x = MatrixP.xx * hanging_velo.x + MatrixP.xy * hanging_velo.y - m_rhs.x;
			Flux_relaxed.y = MatrixP.yx * hanging_velo.x + MatrixP.yy * hanging_velo.y - m_rhs.y;

			double child1_total_energy = m_child1_vara->DouCData[idMass] * m_child1_vara->DouCData[idTotalEnergy_cur];
			double child2_total_energy = m_child2_vara->DouCData[idMass] * m_child2_vara->DouCData[idTotalEnergy_cur];
			double parent_total_energy = m_parent_vara->DouCData[idMass] * m_parent_vara->DouCData[idTotalEnergy_cur];





			/*按总能比例分配Flux_relaxed给父子网格*/
			m_child1_vara->VecCnData[idcnFluxRelaxed][m_which_corner[0]] = child1_total_energy /
				(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
			m_child2_vara->VecCnData[idcnFluxRelaxed][m_which_corner[1]] = child2_total_energy /
				(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
			ParentBounInfo  *PCInfo = (ParentBounInfo  *)&m_parent_data->m_pc_edge_data;
			m_parent_data->m_pc_edge_data[parent_face_index].IsParentChildBoun = true;
			PCInfo[parent_face_index].addDiss = true;
			PCInfo[parent_face_index].Hanging_velocity = hanging_velo;
			PCInfo[parent_face_index].FluxRelaxed = parent_total_energy /
				(child1_total_energy + child2_total_energy + parent_total_energy)*Flux_relaxed;
		}
	}
}
















































































































































































































































































/*p4est定义的which_corner如下图*/
//------------
//2          3|
//|           |
//|           |
//|0         1|
//-------------
//用户自定义的m_index如下图
//------------
//1          2|
//|           |
//|           |
//|0         3|
//-------------
static int convert_which_corner_to_user_define_index(const int &which_corner)
{
	int m_index;
	if (which_corner == p4est_enum_corner::left_bottom) { m_index = quad_data_t::EnumCorner::LEFTBOTTOM; }
	if (which_corner == p4est_enum_corner::right_bottom) { m_index = quad_data_t::EnumCorner::RIGHTBOTTOM; }
	if (which_corner == p4est_enum_corner::left_up) { m_index = quad_data_t::EnumCorner::LEFTUP; }
	if (which_corner == p4est_enum_corner::right_up) { m_index = quad_data_t::EnumCorner::RIGHTUP; }
	return m_index;
}

static int convert_user_define_index_to_which_corner(const int &which_corner)
{
	int m_index;
	if (which_corner == 0) { m_index = 0; }
	if (which_corner == 3) { m_index = 1; }
	if (which_corner == 1) { m_index = 2; }
	if (which_corner == 2) { m_index = 3; }
	return m_index;
}

//           |
//  side[2]  |   side[3]
//           |
//---------corner----------
//           |
//  side[0]  |   side[1]
//           |

void CalculateCornerRcpLcpNcp(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_RcpLcpNcp_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void quadrant_get_BYD_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	CEdge_data		*edata = (CEdge_data *)&data->m_edata;
	p4est_t			*p4est = info->p4est;
	p4est_connectivity_t *conn = p4est->connectivity;
	CHalf_edge_data  *m_plus, *m_minus;
	int			level = info->quad->level;
	int			quadid = info->quadid;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_qcoord_t qx = info->quad->x;
	p4est_qcoord_t qy = info->quad->y;
	p4est_topidx_t	which_tree = info->treeid;


	p4est_qcoord_to_vertex(conn, which_tree, qx, qy, data->init_node_coords[0]);//左下角
	p4est_qcoord_to_vertex(conn, which_tree, qx, qy+length, data->init_node_coords[1]);//左上角
	p4est_qcoord_to_vertex(conn, which_tree, qx+length, qy+length, data->init_node_coords[2]);//右上角
	p4est_qcoord_to_vertex(conn, which_tree, qx+length, qy, data->init_node_coords[3]);//右下角

	double x_length = p4est_data->m_grid_info.global_nx*p4est_data->m_grid_info.tree_width;
	double y_length = p4est_data->m_grid_info.global_ny*p4est_data->m_grid_info.tree_height;
	bool m_left_boundary = false;
	bool m_right_boundary = false;
	bool m_bottom_boundary = false;
	bool m_top_boundary = false;
	
	/*计算tree的坐标索引*/
	int ix = info->treeid%p4est_data->x_tree_number;//x方向索引(0到p4est_data->x_tree_number-1)
	int iy = info->treeid / p4est_data->y_tree_number;//y方向索引(0到p4est_data->y_tree_number-1)

	for (int i = 0; i < CNDIM; i++){
		/*全部给定默认边界为-1*/
		m_plus = (CHalf_edge_data *)&cndata[i].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[i].hdata[CHalf_edge_data::cside::minus];
		m_plus->enumBYD = -1;
		m_minus->enumBYD = -1;
		m_plus->BYDVal = 0.;
		m_minus->BYDVal = 0.;
	}
	//左下角为(qx, qy),右上角为（qx+length,qy+length）

	//左边界，逻辑坐标qx==0
	if (p4est_data->which_case == ProblemNo::TriplePoint)
	{
		if (data->init_node_coords[0][0] == 0)
		{
			m_left_boundary = true;
		}
		if (data->init_node_coords[2][0] == x_length)
		{
			m_right_boundary = true;
		}
		if (data->init_node_coords[0][1] == 0)
		{
			m_bottom_boundary = true;
		}
		if (data->init_node_coords[2][1] == y_length)
		{
			m_top_boundary = true;
		}
	}
	else
	{
		if (qx == 0) { m_left_boundary = true; }
		if (qx == P4EST_ROOT_LEN - length) { m_right_boundary = true; }
		if (qy == 0) { m_bottom_boundary = true; }
		if (qy == P4EST_ROOT_LEN - length) { m_top_boundary = true; }
	}

	if (m_left_boundary)
	{
		cndata[0].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->LeftBoun;
		cndata[1].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->LeftBoun;

		cndata[0].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->LeftBounVal;
		cndata[1].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->LeftBounVal;

		edata[quad_data_t::EnumEdge::LEFT].EdgeType = p4est_data->LeftBoun;
	}

	/*右边界，逻辑坐标qx = P4EST_ROOT_LEN - length*/
	if (m_right_boundary)
		//if(qx===P4EST_ROOT_LEN - length)
	{
		cndata[3].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->RightBoun;
		cndata[2].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->RightBoun;

		cndata[3].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->RightBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->RightBounVal;

		edata[quad_data_t::EnumEdge::RIGHT].EdgeType = p4est_data->RightBoun;
	}

	/*下边界，逻辑坐标qy = 0*/
	if (m_bottom_boundary)
		//if(qy===0)
	{
		cndata[0].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->BottomBoun;
		cndata[3].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->BottomBoun;

		cndata[0].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->BottomBounVal;
		cndata[3].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->BottomBounVal;

		edata[quad_data_t::EnumEdge::BOTTOM].EdgeType = p4est_data->BottomBoun;
	}

	/*上边界，逻辑坐标qy = P4EST_ROOT_LEN - length*/
	if (m_top_boundary)
		//if(qy===P4EST_ROOT_LEN - length)
	{
		cndata[1].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->TopBoun;
		cndata[2].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->TopBoun;

		cndata[1].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->TopBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->TopBounVal;

		edata[quad_data_t::EnumEdge::UP].EdgeType = p4est_data->TopBoun;
	}
}

/*从森林边界条件中获得叶子网格的边界条件*/
static void get_boundary_from_p4est(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

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
		quadrant_get_BYD_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void get_quadrant_boundary_from_p4est(p4est_t *p4est, p4est_quadrant_t *q)
{
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	CCorner_data	*cndata = (CCorner_data *)&data->m_cndata;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;





	for (int i = 0; i < CNDIM; i++) {

		cndata[i].hdata[CHalf_edge_data::cside::minus].enumBYD = -1;
		cndata[i].hdata[CHalf_edge_data::cside::plus].enumBYD = -1;
		cndata[i].hdata[CHalf_edge_data::cside::minus].BYDVal = 0.;
		cndata[i].hdata[CHalf_edge_data::cside::plus].BYDVal = 0.;
	}
	//左下角为(qx, qy),右上角为（qx+length,qy+length）

	//左边界，逻辑坐标qx==0
	if (qx==0)
	{
		cndata[0].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->LeftBoun;
		cndata[1].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->LeftBoun;
		cndata[0].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->LeftBounVal;
		cndata[1].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->LeftBounVal;
	}

	/*右边界，逻辑坐标qx = P4EST_ROOT_LEN - length*/
	if (qx == P4EST_ROOT_LEN - length)
	{
		cndata[3].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->RightBoun;
		cndata[2].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->RightBoun;
		cndata[3].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->RightBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->RightBounVal;
	}

	/*下边界，逻辑坐标qy = 0*/
	if(qy==0)
	{
		cndata[0].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->BottomBoun;
		cndata[3].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->BottomBoun;
		cndata[0].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->BottomBounVal;
		cndata[3].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->BottomBounVal;
	}

	/*上边界，逻辑坐标qy = P4EST_ROOT_LEN - length*/
	if(qy==P4EST_ROOT_LEN - length)
	{
		cndata[1].hdata[CHalf_edge_data::cside::plus].enumBYD = p4est_data->TopBoun;
		cndata[2].hdata[CHalf_edge_data::cside::minus].enumBYD = p4est_data->TopBoun;
		cndata[1].hdata[CHalf_edge_data::cside::plus].BYDVal = p4est_data->TopBounVal;
		cndata[2].hdata[CHalf_edge_data::cside::minus].BYDVal = p4est_data->TopBounVal;
	}
}

static void quadrant_vtk_coord_update_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_connectivity_t *connectivity = info->p4est->connectivity;
	p4est_qcoord_t qx = info->quad->x;
	p4est_qcoord_t qy = info->quad->y;
	int			level = info->quad->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
	p4est_topidx_t	which_tree = info->treeid;
	double			new_node_coords[CNDIM][P4EST_DIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		new_node_coords[cnid][0] = m_vara->VecCnData[idcnCoords_lag][cnid].x;
		new_node_coords[cnid][1] = m_vara->VecCnData[idcnCoords_lag][cnid].y;
	}
	int m_size = connectivity->num_vertices;
	for(int i = 0; i <m_size*3;i++)
	{
		printf("Vertex %d, %lf\n", i, connectivity->vertices[i]);
	}
}

static int Lagrangian_init_refine_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	int			idCPara;
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	if (qx <= 4 * length ||
		qy <= 4 * length)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lagrangian_refine_fixed_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	int			idCPara;
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		idCPara = idCentroidCoord_cur;
		//break;
	default:
		break;
	}



	if (qx <= 12 * length || qy <= 12 * length)
	
	
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lagrangian_coarsen_fixed_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data;
	


	int			idCPara;
	double		parent_gradient;

	/*父网格和子网格的梯度必须都满足误差条件，才能减疏*/

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	//return 0;

	parent_gradient = 0.;
	for (int i = 0; i < P4EST_CHILDREN; i++)
	{
		data = (quad_data_t *)children[i]->p.user_data;
		p4est_qcoord_t qx = children[i]->x;
		p4est_qcoord_t qy = children[i]->y;
		int			level = children[i]->level;
		p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);



		if (qx >= 18 * length || qy >= 18 * length)
		{
			return 1;
		}
	}
	return 0;
}

static int Lagrangian_refine_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *q)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	int			idCPara;
	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;
	int			level = q->level;
	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		idCPara = idCentroidCoord_cur;
		//break;
	default:
		break;
	}










	if (level<p4est_data->minus_level)/*小于最小细化层级，必须加密*/
	{
		return 1;
	}
	if(level>=p4est_data->max_level)/*大于等于最大细化层级，不能加密*/
	{
		return 0;
	}

	if (p4est_data->refine_coarsen_enum == RefineCriteria::Distance)
	{
		double dist = sqrt(pow(m_vara->VecCData[idCentroidCoord_cur].x, 2) +
			pow(m_vara->VecCData[idCentroidCoord_cur].y, 2));
		if (fabs(dist - p4est_data->shock_velocity*p4est_data->current_time) < p4est_data->refine_err)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	if (m_vara->DouCData[idCPara] > p4est_data->refine_err)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lagrangian_coarsen_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data;



	int			idCPara;
	double		parent_gradient;

	/*父网格和子网格的梯度必须都满足误差条件，才能减疏*/

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	//return 0;

	parent_gradient = 0.;
	for (int i = 0; i < P4EST_CHILDREN; i++)
	{
		data = (quad_data_t *)children[i]->p.user_data;
		p4est_qcoord_t qx = children[i]->x;
		p4est_qcoord_t qy = children[i]->y;
		int			level = children[i]->level;
		p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);



		if (data->m_vara.IntCData[idAllowCoarsening] == p4est_data_t::CoarseningEnum::CoarsingNotAllowed)
		{
			return 0;/*不允许减疏*/
		}

		if (level<=p4est_data->minus_level)/*小于最小细化层级，不能减疏*/
		{
			return 0;
		}

		if (level > p4est_data->max_level)/*大于等于最大细化层级，必须减疏*/
		{
			return 1;
		}

		if (p4est_data->refine_coarsen_enum == RefineCriteria::Distance)
		{
			double dist = sqrt(pow(data->m_vara.VecCData[idCentroidCoord_cur].x, 2) +
				pow(data->m_vara.VecCData[idCentroidCoord_cur].y, 2));
			if (fabs(dist - p4est_data->shock_velocity*p4est_data->current_time) > p4est_data->coarsen_error)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}

		if (data->m_vara.DouCData[idCPara] < p4est_data->coarsen_error)
		{
			return 1;
		}
		parent_gradient += data->m_vara.DouCData[idCPara];
	}
	parent_gradient /= P4EST_CHILDREN;
	if (parent_gradient < p4est_data->coarsen_error)
	{
		return 1;
	}
	else {
		return 0;
	}
}

static int Lagrangian_init_coarsen_err_estimate(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	quad_data_t		*data;

	int			idCPara;
	double		parent_gradient;

	/*父网格和子网格的梯度必须都满足误差条件，才能减疏*/

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	default:
		break;
	}

	parent_gradient = 0.;
	for (int i = 0; i < P4EST_CHILDREN; i++)
	{
		data = (quad_data_t *)children[i]->p.user_data;
		p4est_qcoord_t qx = children[i]->x;
		p4est_qcoord_t qy = children[i]->y;
		int			level = children[i]->level;
		p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

		if (level <= p4est_data->minus_level)/*小于最小细化层级，不能减疏*/
		{
			return 0;
		}

		if (level > p4est_data->max_level)/*大于等于最大细化层级，必须减疏*/
		{
			return 1;
		}

		/*大于梯度阈值，不能减疏*/
		if (data->m_vara.DouCData[idCPara] > p4est_data->coarsen_error) { return 0; }
		parent_gradient += data->m_vara.DouCData[idCPara];
	}
	parent_gradient /= P4EST_CHILDREN;
	if (parent_gradient > p4est_data->coarsen_error) { return 0; }/*大于梯度阈值，不能减疏*/
	else { return 1; }
}

static int Lagrangian_coarsen_init_condition(p4est_t *p4est, p4est_topidx_t which_tree,
	p4est_quadrant_t *children[])
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_quadrant_t parent;



	p4est_quadrant_parent(children[0], &parent);

	return 0;
}

static void generate_children_info_from_parent(p4est_data_t *p4est_data, CVariable *m_vara)
{
	/*假设后面要做细化，预先给定children的几何数据和物理数据*/
	/*children*/
	/*----------------------------------*/
	/*|                 |               |*/
	/*|    child3       |      child4   |*/
	/*------------------|---------------|*/
	/*|                 |               |*/
	/*|    child1       |      child2   |*/
	/*------------------|---------------|*/
	CDoubleVector EdgeData[4], CenterData, CenterCoord, CenterVelocity, concave_center;
	CDoubleVector m_parent_coord[CNDIM], m_parent_concave_coord[CNDIM],
		m_parent_velo[CNDIM], m_parent_concave_velo[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_parent_coord[cnid] = m_vara->VecCnData[idcnCoords_lag][cnid];
		m_parent_concave_coord[cnid] = m_vara->VecCnData[idcnCoords_lag][CNDIM-1-cnid];

		m_parent_velo[cnid] = m_vara->VecCnData[idcnVelocity_lag][cnid];
		m_parent_concave_velo[cnid] = m_vara->VecCnData[idcnVelocity_lag][CNDIM - 1 - cnid];
	}

	int IsConcaveQuad = GeometryAlg::is_concave_quad(m_parent_concave_coord);

	if (IsConcaveQuad < 0)
	{
		if (p4est_data->children_center_type == p4est_data_t::center_type::average)
		{
			CenterCoord = 0.25 * (m_parent_coord[0] + m_parent_coord[1] +
				m_parent_coord[2] + m_parent_coord[3]);
		}
		else if (p4est_data->children_center_type == p4est_data_t::center_type::integrated)
		{
			CenterCoord = GeometryAlg::GetPolyCenter(m_parent_coord);
		}
		CenterVelocity = 0.25 * (m_parent_velo[0] + m_parent_velo[1] +
			m_parent_velo[2] + m_parent_velo[3]);
	}
	else
	{
		CenterCoord = GeometryAlg::concave_quad_centroid(IsConcaveQuad, m_parent_concave_coord);
		CenterVelocity = 0.5 * (m_parent_concave_velo[IsConcaveQuad]
			+ m_parent_concave_velo[(IsConcaveQuad+2)%4]);
	}

	enum m_geometry_id { m_coord, m_velo };
	enum m_physical_id {m_density, m_internal_energy};
	enum m_which_child {child1, child2, child3, child4};
	int idCnIndex, idCIndex;
	for (int idChildrenGeomIndex = m_geometry_id::m_coord; idChildrenGeomIndex <= m_geometry_id::m_velo; idChildrenGeomIndex++)
	{
		switch (idChildrenGeomIndex)
		{
		case m_geometry_id::m_coord:
			idCnIndex = idcnCoords_lag;
			CenterData = CenterCoord;
			break;
		case m_geometry_id::m_velo:
			idCnIndex = idcnVelocity_lag;
			CenterData = CenterVelocity;
			break;
		default:
			break;
		}

		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		EdgeData[quad_data_t::EnumEdge::LEFT] = 0.5*
			(m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTBOTTOM] +
				m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTUP]);
		EdgeData[quad_data_t::EnumEdge::BOTTOM] = 0.5*
			(m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTBOTTOM] +
				m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTBOTTOM]);
		EdgeData[quad_data_t::EnumEdge::RIGHT] = 0.5*
			(m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTBOTTOM] +
				m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTUP]);
		EdgeData[quad_data_t::EnumEdge::UP] = 0.5*
			(m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTUP] +
				m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTUP]);






		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		/*child1*/
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::LEFTBOTTOM] =
			m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTBOTTOM];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::LEFTUP] =
			EdgeData[quad_data_t::EnumEdge::LEFT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::RIGHTUP] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child1][quad_data_t::EnumCorner::RIGHTBOTTOM]=
			EdgeData[quad_data_t::EnumEdge::BOTTOM];
		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		/*child2*/
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::LEFTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::BOTTOM];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::LEFTUP] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::RIGHTUP] =
			EdgeData[quad_data_t::EnumEdge::RIGHT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child2][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTBOTTOM];

		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		/*child3*/
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::LEFTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::LEFT];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::LEFTUP] =
			m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::LEFTUP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::RIGHTUP] =
			EdgeData[quad_data_t::EnumEdge::UP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child3][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			CenterData;

		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		/*child4*/
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::LEFTBOTTOM] =
			CenterData;
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::LEFTUP] =
			EdgeData[quad_data_t::EnumEdge::UP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::RIGHTUP] =
			m_vara->VecCnData[idCnIndex][quad_data_t::EnumCorner::RIGHTUP];
		m_vara->ChildrenCnGeomVara[idChildrenGeomIndex][m_which_child::child4][quad_data_t::EnumCorner::RIGHTBOTTOM] =
			EdgeData[quad_data_t::EnumEdge::RIGHT];
	}

	CDoubleVector m_child1_coord[CNDIM], m_child2_coord[CNDIM],
		m_child3_coord[CNDIM], m_child4_coord[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{

		m_child1_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child1][cnid];
		m_child2_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child2][cnid];
		m_child3_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child3][cnid];
		m_child4_coord[cnid] = m_vara->ChildrenCnGeomVara[m_geometry_id::m_coord][m_which_child::child4][cnid];
	}
	double parent_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_parent_coord);
	double child1_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child1_coord);
	double child2_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child2_coord);
	double child3_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child3_coord);
	double child4_volume = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_child4_coord);
	double children_volume = child1_volume + child2_volume + child3_volume + child4_volume;

	if (abs((parent_volume - children_volume) / parent_volume) > m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("The pre-calculated children information is wrong\n");
		abort();
	}

	for (int idChildrenPhysicalIndex = m_physical_id::m_density; idChildrenPhysicalIndex <= m_physical_id::m_internal_energy; idChildrenPhysicalIndex++)
	{
		switch (idChildrenPhysicalIndex)
		{
		case m_physical_id::m_density:
			idCIndex = idDensity_lag;
			break;
		case m_physical_id::m_internal_energy:
			idCIndex = idInternalEnergy_lag;
			break;
		default:
			break;
		}
		for (int childid = 0; childid < P4EST_CHILDREN; childid++)
		{
			m_vara->ChildrenPhysicalVara[idChildrenPhysicalIndex][childid] =
				m_vara->DouCData[idCIndex];
			if (m_vara->ChildrenPhysicalVara[idChildrenPhysicalIndex][childid] > m_eps)
			{
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("The value of ChildrenPysicalVara is illegal in refining!\n");
				abort();
			}
		}
	}
}

/*初始化，给定初始坐标，物理量分布*/
/*1---------2*/
/*----------*/
/*----------*/
/*----------*/
/*0--------3*/
static void Lagrangian_init_condition(p4est_t *p4est, p4est_topidx_t which_tree, p4est_quadrant_t *q)
{

	quad_data_t		*data = (quad_data_t *)q->p.user_data;
	CVariable	*m_vara = (CVariable *)&data->m_vara;
	p4est_connectivity_t *connectivity = p4est->connectivity;
	p4est_data_t			*p4est_data = (p4est_data_t *)p4est->user_pointer;

	p4est_data->coord_type = p4est_data_t::MyCoordType::plane;
	p4est_data->Scheme_type = p4est_data_t::MySchemeType::ControlVolume;


	int			level = q->level;

	p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);

	//计算物理空间中的单元边长（level级细分的边长）
	double dx = 1.0 / (1 << level);


	p4est_qcoord_t qx = q->x;
	p4est_qcoord_t qy = q->y;

	int index_i = int(qx / length);
	int index_j = int(qy / length);
	int width_num = (1 << level);

	//将逻辑坐标转化为物理坐标
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy, data->init_node_coords[0]);//左下角
	p4est_qcoord_to_vertex(connectivity, which_tree, qx, qy + length, data->init_node_coords[1]);//左上角
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy + length, data->init_node_coords[2]);//右上角
	p4est_qcoord_to_vertex(connectivity, which_tree, qx + length, qy, data->init_node_coords[3]);//右下角

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->VecCnData[idcnCoords_cur][i].x = data->init_node_coords[i][0];
		m_vara->VecCnData[idcnCoords_cur][i].y = data->init_node_coords[i][1];
		m_vara->VecCnData[idcnVelocity_cur][i] = CDoubleVector(0.0, 0.0);
		m_vara->VecCnData[idcnVelocity_lag][i] = CDoubleVector(0.0, 0.0);
	}

	CDoubleVector cnCoordCur[CNDIM], cnCoordLag[CNDIM], cnVeloCur[CNDIM], cnVeloLag[CNDIM];
	for (int i = 0; i < CNDIM; i++)
	{
		cnCoordCur[i] = m_vara->VecCnData[idcnCoords_cur][i];
		cnCoordLag[i] = m_vara->VecCnData[idcnCoords_lag][i];
		cnVeloCur[i] = m_vara->VecCnData[idcnVelocity_cur][i];
		cnVeloLag[i] = m_vara->VecCnData[idcnVelocity_lag][i];
	}

	PhysicalAlg::InitCondition(p4est_data->which_case,
		p4est_data->coord_type, int(qx), int(qy), index_i, index_j, width_num,
		cnCoordCur, cnCoordLag, cnVeloCur, cnVeloLag,
		m_vara->DouCData[idDensity_cur],
		m_vara->DouCData[idDensity_lag],
		m_vara->DouCData[idVolume],
		m_vara->DouCData[idMass],
		m_vara->VecCData[idCentroidCoord_cur],
		m_vara->VecCData[idCentroidCoord_lag],
		m_vara->VecCData[idCentroidVelo_cur],
		m_vara->VecCData[idCentroidVelo_lag],
		m_vara->DouCData[idInternalEnergy_cur],
		m_vara->DouCData[idInternalEnergy_lag],
		m_vara->DouCData[idPressure_cur],
		m_vara->DouCData[idPressure_lag],
		m_vara->DouCData[idTotalEnergy_cur],
		m_vara->DouCData[idTotalEnergy_lag],
		m_vara->DouCData[idSoundSpeed],
		m_vara->DouCData[idGamma],
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
		m_vara->VecCnData[idcnCoords_cur][i] = cnCoordCur[i];
		m_vara->VecCnData[idcnCoords_lag][i] = cnCoordLag[i];
		m_vara->VecCnData[idcnVelocity_cur][i] = cnVeloCur[i];
		m_vara->VecCnData[idcnVelocity_lag][i] = cnVeloLag[i];
	}

	generate_children_info_from_parent(p4est_data, m_vara);
}

/*预估时间步长回调函数*/
static void quadrant_predict_timestep_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	CDoubleVector corner_coords[CNDIM];
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		corner_coords[cnid] = m_vara->VecCnData[idcnCoords_cur][cnid];
	}

	if (m_vara->DouCData[idSoundSpeed] < m_eps)
	{

	}
	
	/*1，根据CFL条件得到的dt*/
	double quad_cfl_dt = PhysicalAlg::get_CourantTimeStep(corner_coords, m_vara->DouCData[idSoundSpeed]);

	/*2，根据体积变化率得到的dt*/
	double quad_vol_dt = PhysicalAlg::get_VolumeVarationTimeStep(p4est_data->volume_varation_torelarion,
		m_vara->DouCData[idDivergence]);

	/*3，单个时间步长的增长率不能超过0.01*/
	double quad_increased_dt = p4est_data->delta_time * p4est_data->dt_increase_percent;

	/*min 函数只能两个两个比较*/
	p4est_data->local_dt = min(quad_cfl_dt,
		min(quad_vol_dt, quad_increased_dt));

	if (p4est_data->local_dt < m_eps)
	{
		P4EST_GLOBAL_PRODUCTIONF("Time step is too small in quad %d\n", info->quadid);
		abort();
	}
}









static void 
predict_timestep(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_predict_timestep_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	int		mpiret;
	mpiret =
		sc_MPI_Allreduce(&p4est_data->local_dt, &p4est_data->delta_time,
			1, sc_MPI_DOUBLE, sc_MPI_MAX, p4est->mpicomm);
	SC_CHECK_MPI(mpiret);
}













































/*散度回调函数*/
static void quadrant_compute_divergence_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	/*1.获取当前quadrant的用户指针*/
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	
	/*节点坐标cnCoord, 节点速度cnVelocity*/
	CDoubleVector	cnVelocity[CNDIM];
	CDoubleVector	cnCoord[CNDIM];
	for (int k = 0; k < CNDIM; k++)
	{
		cnCoord[k] = m_vara->VecCnData[idcnCoords_lag][k];
		cnVelocity[k] = m_vara->VecCnData[idcnVelocity_lag][k];
	}
	m_vara->DouCData[idDivergence] = PhysicalAlg::CalculateDivergence(p4est_data->coord_type, cnCoord, cnVelocity);
}

/*计算网格的散度*/
void ComputeDivergence(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_divergence_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

/*声速回调函数*/
static void quadrant_compute_soundspeed_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	m_vara->DouCData[idSoundSpeed] = PhysicalAlg::CalculateSoundSpeed(m_vara->DouCData[idGamma], m_vara->DouCData[idPressure_lag], m_vara->DouCData[idDensity_lag]);
}































































































































































static void 
quadrant_corner_minmod_estimate_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	which_corner, cnid, is_ghost, is_ghost_aside, m_size;
	int			quadid, quadid_aside, idCPara, idCNPara;
	quad_data_t		*m_data, *m_data_aside;
	CVariable		*m_vara, *m_vara_aside;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;
	double			ParaGradient;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idPressure_cur;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idDensity_cur;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	m_size = int(sides->elem_count);

	/*calculate the corner gradient*/
	for (int i = 0; i < m_size; i++)
	{
		/*side[i]包含了该侧数值计算所需要的信息*/
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		/*拿到用户自定义数据*/
		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_vara = (CVariable  *)&m_data->m_vara;

		m_vara->DouCnData[idCNPara][cnid] = 0.;/*初始化为零*/
		for (int j = 0; j < m_size; j++)
		{
			if (j == i) { continue; }
			side[j] = p4est_iter_cside_array_index_int(sides, j);
			quadid_aside = side[j]->quadid;
			is_ghost_aside = side[j]->is_ghost;
			if (is_ghost_aside)
			{
				m_data_aside = (quad_data_t  *)&ghost_data[quadid_aside];
			}
			else
			{
				m_data_aside = (quad_data_t  *)side[j]->quad->p.user_data;
			}
			m_vara_aside = (CVariable  *)&m_data_aside->m_vara;

			double m_dist = GeometryAlg::GetPointToPointDistance(
				m_vara->VecCData[idCentroidCoord_cur], m_vara_aside->VecCData[idCentroidCoord_cur]);
			ParaGradient = abs(m_vara->DouCData[idCPara] - m_vara_aside->DouCData[idCPara]) / m_dist;
			m_vara->DouCnData[idCNPara][cnid] = SC_MAX(m_vara->DouCnData[idCNPara][cnid], ParaGradient);
		}
	}
}

static void
quadrant_whether_allowing_coarsening_from_corner_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	is_ghost_a, m_size;
	int			quadid_a, quadid_b;
	quad_data_t		*m_data_a;
	CVariable		*m_vara_a;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;

	m_size = int(sides->elem_count);

	for (int i = 0; i < m_size; i++)
	{
		/*side[i]包含了该侧数值计算所需要的信息*/
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid_a = side[i]->quadid;
		p4est_quadrant	*quad_a = side[i]->quad;
		int level_a = quad_a->level;

		is_ghost_a = side[i]->is_ghost;
		if (is_ghost_a)
		{
			m_data_a = (quad_data_t  *)&ghost_data[quadid_a];
		}
		else
		{
			m_data_a = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_vara_a = (CVariable  *) &m_data_a->m_vara;
		for (int j = 0; j < m_size; j++)
		{
			if (j == i) { continue; }
			side[j] = p4est_iter_cside_array_index_int(sides, j);
			quadid_b = side[j]->quadid;
			p4est_quadrant	*quad_b = side[j]->quad;
			int level_b = quad_b->level;
			
			if (level_b - level_a > 1)
			{
				m_vara_a->IntCData[idAllowCoarsening] = p4est_data_t::CoarseningEnum::CoarsingNotAllowed;
			}
		}
	}
}

/*for two quadrants on either side of a face, estimate the derivate across the face*/
static void quadrant_edge_minmod_estimate_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				idCPara, idEPara;
	int				m_which_corner[2], m_master_corner[2], 
		m_unconstrained_master_corner[2], m_which_side[2];

	if (sides->elem_count != 2) { return; }
	P4EST_ASSERT(sides->elem_count == 2);

	/*用压力或者密度的梯度作为细化和减疏的简单准则*/
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idPressure_cur;
		idEPara = idEPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idDensity_cur;
		idEPara = idERhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	/*悬点边*/
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1-i] = p4est_iter_fside_array_index_int(sides, 1-i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				/*|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]*/) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}
			double		parent_para, child1_para, child2_para;
			double		parent_gradient, child1_gradient, child2_gradient;
			double		dist1, dist2;
			CDoubleVector	parent_center, child1_center, child2_center;
			int				children_face, parent_face;

			/*网格中心的物理量，如密度或者压力*/
			parent_para = m_parent_vara->DouCData[idCPara];
			child1_para = m_child1_vara->DouCData[idCPara];
			child2_para = m_child2_vara->DouCData[idCPara];

			/*网格中心坐标*/
			parent_center = m_parent_vara->VecCData[idCentroidCoord_cur];
			child1_center = m_child1_vara->VecCData[idCentroidCoord_cur];
			child2_center = m_child2_vara->VecCData[idCentroidCoord_cur];

			/*父子网格中心点的距离*/
			dist1 = GeometryAlg::GetPointToPointDistance(parent_center, child1_center);
			dist2 = GeometryAlg::GetPointToPointDistance(parent_center, child2_center);

			/*求解物理量梯度*/
			child1_gradient = abs(parent_para - child1_para) / dist1;
			child2_gradient = abs(parent_para - child2_para) / dist2;
			parent_gradient = (child1_gradient + child2_gradient) / 2.;

			/*面的序号*/
			children_face = side[i]->face;
			parent_face = side[1 - i]->face;

			m_child1_vara->DouEData[idEPara][children_face] = child1_gradient;
			m_child2_vara->DouEData[idEPara][children_face] = child2_gradient;
			m_parent_vara->DouEData[idEPara][parent_face] = parent_gradient;
		}
	}

	/*非悬点边*/
	side[0] = p4est_iter_fside_array_index_int(sides, 0);
	side[1] = p4est_iter_fside_array_index_int(sides, 1);
	if (!(side[0]->is_hanging) && !(side[1]->is_hanging))
	{
		double	m_para[2];
		double	m_gradient;
		CDoubleVector	m_center[2];
		int				face_index[2];

		p4est_quadrant_t		*brother1_quad, *brother2_quad;
		quad_data_t				*brother1_data, *brother2_data;
		brother1_quad = side[0]->is.full.quad;
		brother2_quad = side[1]->is.full.quad;

		face_index[0] = side[0]->face;

		/*界面两侧单元数据brother1_data， brother2_data*/
		if (side[0]->is.full.is_ghost)
		{
			brother1_data = &ghost_data[side[0]->is.full.quadid];
		}
		else
		{
			brother1_data = (quad_data_t  *)side[0]->is.full.quad->p.user_data;
		}

		/*边界处梯度默认为0*/
		if (side[1]->is.full.quad == NULL || info->sides.elem_count <2 ||
			side[1]->is.full.quadid>info->p4est->global_num_quadrants)
		{
			brother1_data->m_vara.DouEData[idEPara][face_index[0]] = 0.;
			return;
		}

		face_index[1] = side[1]->face;
		if (side[1]->is.full.is_ghost)
		{
			brother2_data = &ghost_data[side[1]->is.full.quadid];
		}
		else if (!(side[1]->is.full.quad))
		{
			brother1_data->m_vara.DouEData[idEPara][face_index[0]] = 0.;
			return;
		}
		else
		{
			brother2_data = (quad_data_t  *)side[1]->is.full.quad->p.user_data;
		}

		m_para[0] = brother1_data->m_vara.DouCData[idCPara];
		m_para[1] = brother2_data->m_vara.DouCData[idCPara];
		m_center[0] = brother1_data->m_vara.VecCData[idCentroidCoord_cur];
		m_center[1] = brother2_data->m_vara.VecCData[idCentroidCoord_cur];

		double dist = GeometryAlg::GetPointToPointDistance(m_center[0], m_center[1]);
		m_gradient = abs(m_para[0] - m_para[1]) / dist;

		brother1_data->m_vara.DouEData[idEPara][face_index[0]] = m_gradient;
		brother2_data->m_vara.DouEData[idEPara][face_index[1]] = m_gradient;
	}
}

/*update at the level boundary after coarsening, to ensure the consrain condition and conservation*/
static void quadrant_update_after_coarsening_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2],
		m_unconstrained_master_corner[2], m_which_side[2];

	/*悬点边*/
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants) {

				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector  master_coord[2], master_velo[2], middle_coord, middle_velo,
				child1_cn_coord, child2_cn_coord, child1_cn_velo, child2_cn_velo;

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
				break;
			}
			middle_coord = 0.5*(master_coord[0] + master_coord[1]);
			middle_velo = 0.5*(master_velo[0] + master_velo[1]);

			child1_cn_coord = m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]];
			child2_cn_coord = m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]];

			child1_cn_velo = m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]];
			child2_cn_velo = m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]];

			double dist1, dist2, delta_velo1, delta_velo2;
			dist1 = GeometryAlg::GetPointToPointDistance(middle_coord, child1_cn_coord);
			dist2 = GeometryAlg::GetPointToPointDistance(middle_coord, child2_cn_coord);
			delta_velo1 = GeometryAlg::GetPointToPointDistance(middle_velo, child1_cn_velo);
			delta_velo2 = GeometryAlg::GetPointToPointDistance(middle_velo, child2_cn_velo);

			/*粗化后，细网格的节点不在粗网格边界的中点，则对细网格进行重分重映*/
			if (delta_velo1 >= m_coliner_eps || delta_velo2 >= m_coliner_eps)/*粗细网格是否共线的判断*/
			{
				CDoubleVector  m_cell_coord[CNDIM];

				/************************for child 1***************************/
				m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]] = middle_coord;/*1,坐标*/
				m_child1_vara->VecCnData[idcnCoords_lag][m_which_corner[0]] = m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]];
				
				m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]] = middle_velo;/*2,速度*/
				m_child1_vara->VecCnData[idcnVelocity_lag][m_which_corner[0]] = m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]];

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child1_vara->VecCnData[idcnCoords_cur][i]; }
				m_child1_vara->DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child1_vara->DouCData[idDensity_cur] = m_child1_vara->DouCData[idMass] / m_child1_vara->DouCData[idVolume];

				m_child1_vara->DouCData[idPressure_cur] = PhysicalAlg::EquationOfState(
					m_child1_vara->DouCData[idGamma],
					m_child1_vara->DouCData[idDensity_cur],
					m_child1_vara->DouCData[idInternalEnergy_cur]);/*4，压力*/

																   /************************for child 1***************************/
				m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]] = middle_coord;/*1,坐标*/
				m_child2_vara->VecCnData[idcnCoords_lag][m_which_corner[1]] = m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]];

				m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]] = middle_velo;/*2,速度*/
				m_child2_vara->VecCnData[idcnVelocity_lag][m_which_corner[1]] = m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]];

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child2_vara->VecCnData[idcnCoords_cur][i]; }
				m_child2_vara->DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child2_vara->DouCData[idDensity_cur] = m_child2_vara->DouCData[idMass] / m_child2_vara->DouCData[idVolume];

				m_child2_vara->DouCData[idPressure_cur] = PhysicalAlg::EquationOfState(
					m_child2_vara->DouCData[idGamma],
					m_child2_vara->DouCData[idDensity_cur],
					m_child2_vara->DouCData[idInternalEnergy_cur]);/*4，压力*/
			}
		}
	}
}

static void quadrant_whether_allowing_coarsening_from_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_parent_vara;
	CCorner_data	*m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);

	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	/*悬点边*/
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			/*if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants) {

				continue;
			}*/
			int			childlevel = quad_child1->level;
			int			parentlevel = quad_parent->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(childlevel);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;





























			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			if ((childlevel - parentlevel) > 1)
			{
				/*细化后，相邻网格层级大于1，不允许父网格再粗化了*/
				m_parent_vara->IntCData[idAllowCoarsening] = p4est_data_t::CoarseningEnum::CoarsingNotAllowed;
			}
		}
	}
}

/*update at the level boundary after coarsening, to ensure the constrain condition and conservation*/
static void quadrant_update_after_balance_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_child1_vara, *m_child2_vara, *m_parent_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata, *m_parent_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2],
		m_unconstrained_master_corner[2], m_which_side[2];
	if (sides->elem_count != 2) { return; }
	/*悬点边*/
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		side[1 - i] = p4est_iter_fside_array_index_int(sides, 1 - i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {

				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;

			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
				m_parent_vara = (CVariable *)&ghost_data[side[full_index]->is.full.quadid].m_vara;
				m_parent_cndata = (CCorner_data *)&ghost_data[side[full_index]->is.full.quadid].m_cndata;
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
				m_parent_vara = (CVariable *)&m_parent_data->m_vara;
				m_parent_cndata = (CCorner_data *)&m_parent_data->m_cndata;
			}

			CDoubleVector  master_coord[2], master_velo[2], middle_coord, middle_velo,
				child1_cn_coord, child2_cn_coord, child1_cn_velo, child2_cn_velo;

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTBOTTOM];
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];

				master_velo[0] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::LEFTUP];
				master_velo[1] = m_parent_data->m_vara.VecCnData[idcnVelocity_lag][quad_data_t::EnumCorner::RIGHTUP];
				break;
			}
			middle_coord = 0.5*(master_coord[0] + master_coord[1]);
			middle_velo = 0.5*(master_velo[0] + master_velo[1]);

			child1_cn_coord = m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]];
			child2_cn_coord = m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]];

			child1_cn_velo = m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]];
			child2_cn_velo = m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]];

			double dist1, dist2, delta_velo1, delta_velo2;
			dist1 = GeometryAlg::GetPointToPointDistance(middle_coord, child1_cn_coord);
			dist2 = GeometryAlg::GetPointToPointDistance(middle_coord, child2_cn_coord);
			delta_velo1 = GeometryAlg::GetPointToPointDistance(middle_velo, child1_cn_velo);
			delta_velo2 = GeometryAlg::GetPointToPointDistance(middle_velo, child2_cn_velo);

			/*粗化后，细网格的节点不在粗网格边界的中点，则对细网格进行重分重映*/
			if (delta_velo1 >= m_coliner_eps || delta_velo2 >= m_coliner_eps)/*粗细网格是否共线的判断*/
			{
				CDoubleVector  m_cell_coord[CNDIM];

				/************************for child 1***************************/
				m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]] = middle_coord;/*1,坐标*/
				m_child1_vara->VecCnData[idcnCoords_lag][m_which_corner[0]] = m_child1_vara->VecCnData[idcnCoords_cur][m_which_corner[0]];

				m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]] = middle_velo;/*2,速度*/
				m_child1_vara->VecCnData[idcnVelocity_lag][m_which_corner[0]] = m_child1_vara->VecCnData[idcnVelocity_cur][m_which_corner[0]];

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child1_vara->VecCnData[idcnCoords_cur][i]; }
				m_child1_vara->DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child1_vara->DouCData[idDensity_cur] = m_child1_vara->DouCData[idMass] / m_child1_vara->DouCData[idVolume];

				m_child1_vara->DouCData[idPressure_cur] = PhysicalAlg::EquationOfState(
					m_child1_vara->DouCData[idGamma],
					m_child1_vara->DouCData[idDensity_cur],
					m_child1_vara->DouCData[idInternalEnergy_cur]);/*4，压力*/

																   /************************for child 2***************************/
				m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]] = middle_coord;/*1,坐标*/
				m_child2_vara->VecCnData[idcnCoords_lag][m_which_corner[1]] = m_child2_vara->VecCnData[idcnCoords_cur][m_which_corner[1]];

				m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]] = middle_velo;/*2,速度*/
				m_child2_vara->VecCnData[idcnVelocity_lag][m_which_corner[1]] = m_child2_vara->VecCnData[idcnVelocity_cur][m_which_corner[1]];

				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_child2_vara->VecCnData[idcnCoords_cur][i]; }
				m_child2_vara->DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
				m_child2_vara->DouCData[idDensity_cur] = m_child2_vara->DouCData[idMass] / m_child2_vara->DouCData[idVolume];

				m_child2_vara->DouCData[idPressure_cur] = PhysicalAlg::EquationOfState(
					m_child2_vara->DouCData[idGamma],
					m_child2_vara->DouCData[idDensity_cur],
					m_child2_vara->DouCData[idInternalEnergy_cur]);/*4，压力*/
			}
		}
	}
}

static void quadrant_cell_minmod_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data=(quad_data_t		*)info->quad->p.user_data;
	CVariable		*m_vara=(CVariable		*)&data->m_vara;
	p4est_t			*p4est = info->p4est;

	int				idCPara, idEPara, idCNPara;
	
	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		idEPara = idEPressureGradient;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		idEPara = idERhoGradient;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	m_vara->DouCData[idCPara] = 0.;

	/*the gradient of the cell is adopted as the max value of the gradient across the edges*/
	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->DouCData[idCPara] = SC_MAX(m_vara->DouCData[idCPara], m_vara->DouEData[idEPara][i]);
	}





}

/*计算声速*/
void ComputeSoundSpeed(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_soundspeed_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

/*计算时间半步物理量的回调函数*/
static void quadrant_compute_halftime_variable_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	
	quad_data_t		*data = (quad_data_t		*)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable		*)&data->m_vara;
	p4est_qcoord_t	qx = info->quad->x;
	p4est_qcoord_t	qy = info->quad->y;
	m_vara->VecCData[idCentroidVelo_half] = (m_vara->VecCData[idCentroidVelo_cur] + m_vara->VecCData[idCentroidVelo_lag]) / 2.;
	m_vara->VecCData[idCentroidCoord_half] = (m_vara->VecCData[idCentroidCoord_cur] + m_vara->VecCData[idCentroidCoord_lag]) / 2.;

	/*动力学量*/
	for (int i = 0; i < CNDIM; i++)
	{

		m_vara->VecCnData[idcnCoords_half][i] = GeometryAlg::GetPointPointMiddle(m_vara->VecCnData[idcnCoords_cur][i], m_vara->VecCnData[idcnCoords_lag][i]);
	}
	/*由动力学量计算得到密度半步的计算方式*/
	m_vara->DouCData[idDensity_half] = 2.*m_vara->DouCData[idDensity_cur] * m_vara->DouCData[idDensity_lag] /
		(m_vara->DouCData[idDensity_cur] + m_vara->DouCData[idDensity_lag]);
	/*热力学量*/
	m_vara->DouCData[idTotalEnergy_half] = (m_vara->DouCData[idTotalEnergy_cur]+ m_vara->DouCData[idTotalEnergy_lag]) / 2.;
	m_vara->DouCData[idInternalEnergy_half] = (m_vara->DouCData[idInternalEnergy_cur] + m_vara->DouCData[idInternalEnergy_lag]) / 2.;
	if (m_vara->DouCData[idInternalEnergy_half] > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The half time idinternalenergy is illegal\n", P4EST_DIM);
		abort();
	}
	m_vara->DouCData[idPressure_half] = PhysicalAlg::EquationOfState(m_vara->DouCData[idGamma], m_vara->DouCData[idDensity_half], m_vara->DouCData[idInternalEnergy_half]);
	if (m_vara->DouCData[idPressure_half] > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("The half time pressure is illegal\n", P4EST_DIM);
		abort();
	}
}

void CalculateHalfTimeVariable(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;


	p4est_iterate(p4est,
		NULL,
		(void*)p4est_data,
		quadrant_compute_halftime_variable_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

/*AMR父网格边界，边中心Mcp回调函数，用于求解边中点的Fcp*/
static void quadrant_parent_edge_matrix_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t		*)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable		*)&data->m_vara;
	ParentBounInfo	*PCInfo = (ParentBounInfo	*)&data->m_pc_edge_data;
	p4est_data_t *p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	CDoubleVector DeltaU[CNDIM], RcpLcpNcpPc[CNDIM];
	CDoubleMatrix  NcpPlusMatrix, NcpMinusMatrix;

	for (int k = 0; k < CNDIM; k++)
	{
		/*只有在父子网格边界才起作用*/
		if (PCInfo[k].IsParentChildBoun == true)
		{
			double Divergence = 0.;
			CDoubleVector	LcpNcpPc, LcpNcp;
			m_vara->DouCnData[idReconstructDensity][k] = m_vara->DouCData[idDensity_cur];
			m_vara->DouCnData[idReconstructPressure][k] = m_vara->DouCData[idPressure_cur];
			m_vara->VecCnData[idReconstructVelocity][k] = m_vara->VecCData[idCentroidVelo_cur];
			double Tc = 0.;
			DeltaU[k] = PCInfo[k].Hanging_velocity - m_vara->VecCnData[idReconstructVelocity][k];
			LcpNcp = PCInfo[k].Lcp[0] * PCInfo[k].Ncp[0] + PCInfo[k].Lcp[1] * PCInfo[k].Ncp[1];
			LcpNcpPc = LcpNcp * m_vara->DouCnData[idReconstructPressure][k];
			Divergence = LcpNcpPc ^ DeltaU[k];
			if (Divergence < -1e-10) { Tc = 1.44; }
			PCInfo[k].Zcp = m_vara->DouCnData[idReconstructDensity][k] * m_vara->DouCData[idSoundSpeed];
			NcpPlusMatrix = GeometryAlg::DyadicProduct(PCInfo[k].Ncp[0], PCInfo[k].Ncp[0]);
			NcpMinusMatrix = GeometryAlg::DyadicProduct(PCInfo[k].Ncp[1], PCInfo[k].Ncp[1]);
			m_vara->MarCnData[ideMcp][k] = PCInfo[k].Zcp * PCInfo[k].Lcp[0] * NcpPlusMatrix
				+ PCInfo[k].Zcp * PCInfo[k].Lcp[1] * NcpMinusMatrix;
			m_vara->VecCnData[ideMcpUc][k] = GeometryAlg::MatrixDotVector(m_vara->MarCnData[ideMcp][k],
				m_vara->VecCnData[idReconstructVelocity][k]);
			m_vara->VecCnData[ideRHS][k] = LcpNcpPc + m_vara->VecCnData[ideMcpUc][k];
		}
	}
}

static void quadrant_corner_matrix_assemble_callback(p4est_iter_volume_info_t *info, void *user_data)
{

	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	CCorner_data	*cndata = (CCorner_data *)&(data->m_cndata);
	p4est_data_t	*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int				CoordType = p4est_data->coord_type;
	int				Scheme_type = p4est_data->Scheme_type;
	CDoubleVector DeltaU[CNDIM], RcpLcpNcpPc[CNDIM];
	CDoubleMatrix  NcpPlusMatrix, NcpMinusMatrix;
	CHalf_edge_data	*m_plus, *m_minus;
	double abs_deltau;

	for (int k = 0; k < CNDIM; k++)
	{
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		double Divergence = 0.;
		CDoubleVector	LcpNcpPc, LcpNcp;

		m_vara->DouCnData[idReconstructDensity][k] = m_vara->DouCData[idDensity_cur];
		m_vara->DouCnData[idReconstructPressure][k] = m_vara->DouCData[idPressure_cur];
		double Tc = 0.;
		m_vara->VecCnData[idReconstructVelocity][k] = m_vara->VecCData[idCentroidVelo_cur];
		DeltaU[k] = m_vara->VecCnData[idcnVelocity_lag][k] - m_vara->VecCnData[idReconstructVelocity][k];
		abs_deltau = sqrt(pow(DeltaU[k].x, 2) + pow(DeltaU[k].y, 2));
		LcpNcp = m_plus->Lcp * m_plus->Ncp + m_minus->Lcp*m_minus->Ncp;
		LcpNcpPc = LcpNcp * m_vara->DouCnData[idReconstructPressure][k];
		Divergence = LcpNcpPc ^ DeltaU[k];

		if (CoordType == p4est_data_t::MyCoordType::plane)/*平面坐标*/
		{
			RcpLcpNcpPc[k] = m_plus->Rcp*m_plus->Lcp * m_plus->Ncp + 
				m_minus->Rcp*m_minus->Lcp*m_minus->Ncp;
		}
		if (CoordType == p4est_data_t::MyCoordType::cylinder)/*柱坐标*/
		{

		}

		m_plus->Zcp = m_vara->DouCnData[idReconstructDensity][k] * m_vara->DouCData[idSoundSpeed];
		m_minus->Zcp = m_vara->DouCnData[idReconstructDensity][k] * m_vara->DouCData[idSoundSpeed];
		m_plus->delta_u_cp = m_vara->VecCnData[idcnVelocity_lag][k] - m_vara->VecCnData[idReconstructVelocity][k];
		m_minus->delta_u_cp = m_vara->VecCnData[idcnVelocity_lag][k] - m_vara->VecCnData[idReconstructVelocity][k];
		m_plus->Uc_cur = m_vara->VecCnData[idReconstructVelocity][k];
		m_minus->Uc_cur = m_vara->VecCnData[idReconstructVelocity][k];
		m_plus->pi = m_vara->DouCnData[idReconstructPressure][k] - m_plus->Zcp * (m_plus->delta_u_cp^ m_plus->Ncp);
		m_minus->pi = m_vara->DouCnData[idReconstructPressure][k] - m_minus->Zcp * (m_minus->delta_u_cp^ m_minus->Ncp);

		NcpPlusMatrix = GeometryAlg::DyadicProduct(m_plus->Ncp, m_plus->Ncp);
		NcpMinusMatrix = GeometryAlg::DyadicProduct(m_minus->Ncp, m_minus->Ncp);

		/*计算隅角矩阵*/
		m_vara->MarCnData[idcnMcp][k] = m_plus->Zcp*m_plus->Rcp*m_plus->Lcp*NcpPlusMatrix
			+ m_minus->Zcp*m_minus->Rcp*m_minus->Lcp*NcpMinusMatrix;

		/*旋转Riemann解，需要修改Mcp矩阵定义*/
		if (p4est_data->solver_type == p4est_data_t::RiemannSolver::Rotated)
		{
			if (abs_deltau > m_eps)
			{
				m_vara->MarCnData[idcnMcp][k].xx = m_plus->Zcp*m_plus->Rcp*m_plus->Lcp *
					abs(DeltaU[k]^ m_plus->Ncp) / abs_deltau +
					m_minus->Zcp*m_minus->Rcp*m_minus->Lcp *
					abs(DeltaU[k] ^ m_minus->Ncp) / abs_deltau;
				m_vara->MarCnData[idcnMcp][k].yy = m_vara->MarCnData[idcnMcp][k].xx;
				m_vara->MarCnData[idcnMcp][k].xy = 0.;
				m_vara->MarCnData[idcnMcp][k].yx = 0.;
			}
		}






















		m_vara->VecCnData[idcnMcpUc][k] = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnMcp][k],
			m_vara->VecCnData[idReconstructVelocity][k]);
		m_vara->VecCnData[idcnRHS][k] = LcpNcpPc + m_vara->VecCnData[idcnMcpUc][k];
	}
}

/*将网格的矩阵Mcp装配到各个节点上，形成节点矩阵Mp*/
static void
quadrant_corner_to_point_matrix_assemble_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_iter_corner_side_t	*side[CNDIM];
	sc_array_t	*sides = &(info->sides);
	int	which_corner, cnid, is_ghost, m_size;
	int			quadid;
	bool		is_boundary;
	int			tree_boundary;
	quad_data_t		*m_data;
	CVariable		*m_vara;
	CDoubleMatrix	MatrixP = CDoubleMatrix(0., 0., 0., 0.);
	CDoubleVector	RHS = CDoubleVector(0., 0.);
	CPointBounInfo	OneBounPlus, OneBounMinus;
	quad_data_t		*ghost_data = (quad_data_t  *)user_data;

	m_size = int(sides->elem_count);

	for (int i = 0; i < m_size; i++)
	{
		/*side[i]包含了该侧数值计算所需要的信息*/
		side[i] = p4est_iter_cside_array_index_int(sides, i);












		quadid = side[i]->quadid;

		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		/*拿到用户自定义数据*/
		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_vara = (CVariable  *)&m_data->m_vara;

		/*根据算法操作用户自定义数据*/
		/*MatrixP累加，RHS累加*/
		MatrixP += m_vara->MarCnData[idcnMcp][cnid];
		RHS += m_vara->VecCnData[idcnRHS][cnid];
	}

	for (int i = 0; i < m_size; i++)
	{
		/*将MatrixP和RHS赋值给各个Points数据*/
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_data->points[cnid].MatrixP = MatrixP;
		m_data->points[cnid].RHS = RHS;
	}

	tree_boundary = info->tree_boundary;


	m_size = int(sides->elem_count);
	CHalf_edge_data	*m_plus, *m_minus;
	is_boundary = false;
	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t  *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t  *)side[i]->quad->p.user_data;
		}
		m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];
		if (m_plus->enumBYD != InnerBoundary) { is_boundary = true; }
		if (m_minus->enumBYD != InnerBoundary) { is_boundary = true; }
	}

	/*如果是边界隅角，对应的节点必须有两条边界边，将信息存储至TwoBouns中*/

	if (is_boundary)
	{
		vector<CPointBounInfo> m_bouns;
		CHalf_edge_data	*m_plus, *m_minus;
		m_size = int(sides->elem_count);
		for (int i = 0; i < m_size; i++)
		{
			side[i] = p4est_iter_cside_array_index_int(sides, i);
			quadid = side[i]->quadid;
			which_corner = side[i]->corner;
			cnid = convert_which_corner_to_user_define_index(which_corner);

			is_ghost = side[i]->is_ghost;
			if (is_ghost)
			{
				m_data = (quad_data_t  *)&ghost_data[quadid];
			}
			else
			{
				m_data = (quad_data_t  *)side[i]->quad->p.user_data;
			}
			m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
			m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];

			if (m_plus->enumBYD != InnerBoundary)
			{
				OneBounPlus.enumType = m_plus->enumBYD;
				OneBounPlus.Val = m_plus->BYDVal;
				OneBounPlus.Ncp = m_plus->Ncp;
				OneBounPlus.Lcp = m_plus->Lcp;
				m_bouns.push_back(OneBounPlus);
			}
			if (m_minus->enumBYD != InnerBoundary)
			{
				OneBounMinus.enumType = m_minus->enumBYD;
				OneBounMinus.Val = m_minus->BYDVal;
				OneBounMinus.Ncp = m_minus->Ncp;
				OneBounMinus.Lcp = m_minus->Lcp;
				m_bouns.push_back(OneBounMinus);
			}
		}
		if (m_bouns.size() != 2)
		{
			P4EST_GLOBAL_PRODUCTIONF("WARNING:boundary number must be 2!\n");
		}
		else
		{
			for (int i = 0; i < m_size; i++)
			{
				side[i] = p4est_iter_cside_array_index_int(sides, i);
				quadid = side[i]->quadid;
				which_corner = side[i]->corner;
				cnid = convert_which_corner_to_user_define_index(which_corner);

				is_ghost = side[i]->is_ghost;
				if (is_ghost)
				{
					m_data = (quad_data_t  *)&ghost_data[quadid];
				}
				else
				{
					m_data = (quad_data_t  *)side[i]->quad->p.user_data;
				}

				m_data->points[cnid].TwoBouns[0] = m_bouns[0];
				m_data->points[cnid].TwoBouns[1] = m_bouns[1];
			}
		}
		m_bouns.clear();
	}
}

/*[in]边界相关的信息，包括边界类型，边界值，边界的外法向量，边界长度*/
/*[out]边界速度*/
/*下图表明节点P对应网格的两条边，Plus和Minus*/
/*       Minus        */
/*-------------------P*/
/*                   |*/
/*                   |*/
/*                   |Plus*/
/*                   |*/
/*                   |*/

/*enumPlus和enumMinus为边界类型*/
/*ValPlus和ValMinus为边界压力/速度值*/
/*NcpPlus[P4EST_DIM]和NcpMinus[P4EST_DIM]为边界单位外法向量*/
/*LcpPlus和LcpMinus为边界边的长度*/
/*MatrixP[P4EST_DIM][P4EST_DIM]为节点求解器的节点矩阵*/
/*m_RHS[P4EST_DIM]为节点求解器的右端项*/
/*[out] velocity为节点速度*/
CDoubleVector BoundaryNodeVelocityComputation(const CPointBounInfo &BounPlus,
	const CPointBounInfo &BounMinus, const CDoubleMatrix MatrixP, const CDoubleVector m_RHS)
{
	int				enumPlus = BounPlus.enumType;
	int				enumMinus = BounMinus.enumType;
	double			ValPlus = BounPlus.Val;
	double			ValMinus = BounMinus.Val;
	CDoubleVector	NcpPlus = BounPlus.Ncp;
	CDoubleVector	NcpMinus = BounMinus.Ncp;
	double			LcpPlus = BounPlus.Lcp;
	double			LcpMinus = BounMinus.Lcp;
	CDoubleVector	velocity = CDoubleVector(0., 0.);


	/*定速度边界条件，不需要求解*/
	if (VelocityBoundary == enumPlus || VelocityBoundary == enumMinus
		||CircleCenterBoundary == enumPlus || CircleCenterBoundary == enumMinus) { return velocity; }
	CDoubleMatrix MatrixPInverse;
	MatrixPInverse = GeometryAlg::MatrixInverse(MatrixP);

	double aa, bb, cc, dd, na, nb, ma, mb, ma1, mb1, PIStar, cos_theta, sin_theta;

	aa = MatrixPInverse.xx;
	bb = MatrixPInverse.xy;
	cc = MatrixPInverse.yx;
	dd = MatrixPInverse.yy;
	na = m_RHS.x;
	nb = m_RHS.y;
	ma = NcpPlus.x * LcpPlus;
	ma1 = NcpMinus.x * LcpMinus;
	mb = NcpPlus.y * LcpPlus;
	mb1 = NcpMinus.y * LcpMinus;
	cos_theta = NcpPlus.x;
	sin_theta = NcpPlus.y;

	bool IsSolved, IsColinear;
	IsSolved = false;

	/*Plus压力+Minus压力边界条件*/
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.x = aa * (ma*ValPlus + ma1*ValMinus + na) +
				bb*(mb*ValPlus + mb1*ValMinus + nb);
			velocity.y = cc* (ma*ValPlus + ma1*ValMinus + na) +
				dd*(mb*ValPlus + mb1*ValMinus + nb);
			IsSolved = true;
		}
	}

	/*Plus速度+Minus速度边界条件*/
	/*速度边界条件只考虑沿x(z)轴，或y(r)轴，或沿射线网格指向球心这几种情况，后期再作扩展*/
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			IsColinear = false;
			if (fabs(NcpPlus.x * NcpMinus.y - NcpPlus.y * NcpMinus.x) < 1e-10) { IsColinear = true; }

			/**************************共线速度边界条件*****************************/
			if (IsColinear)
			{
				/*+++++++++++++沿y（r）方向定速度++++++++++++++=*/
				if (fabs(NcpPlus.x) < 1e-12)
				{
					velocity.y = ValPlus;
					if (fabs(ma*cc + mb*dd) > 1e-12)
					{
						PIStar = (ValPlus - na*cc - nb*dd) / (ma*cc + mb*dd);
					}
					else
					{
						PIStar = 0.0;
					}
					velocity.x = aa*(ma*PIStar + na) + bb*(mb*PIStar + nb);
				}

				/*+++++++++++++沿x（z）方向定速度++++++++++++++=*/
				else if (fabs(NcpPlus.y) < 1e-12)
				{
					velocity.x = ValPlus;
					if (fabs(ma*aa + mb*bb) > 1e-12)
					{
						PIStar = (ValPlus - na*aa - nb*bb) / (ma*aa + mb*bb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.y = cc*(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
				else
				{
					/*+++++++++++沿任意方向定速度+++++++++++++*/
					/*cos_theta*(aa*(ma*PIStar+na) + bb *(mb*PIStar+nb))+sin_theta*(cc*(ma*PIStar+na)+dd*(mb*PIStar+nb)) = ValPlus*/
					if (fabs(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb) > 1e-12)
					{
						PIStar = (ValPlus - cos_theta*aa*na - cos_theta*bb*nb - sin_theta*cc*na - sin_theta*dd*nb) /
							(cos_theta*aa*ma + cos_theta*bb*mb + sin_theta*cc*ma + sin_theta*dd*mb);
					}
					else
					{
						PIStar = 0.;
					}
					velocity.x = aa *(ma*PIStar + na) + bb*(mb*PIStar + nb);
					velocity.y = cc *(ma*PIStar + na) + dd*(mb*PIStar + nb);
				}
			}
			else
			{
				/**************************不共线速度边界条件*****************************/
				velocity = ValPlus*NcpPlus + ValMinus*NcpMinus;
			}
		}
		IsSolved = true;
	}

	/*Plus压力+Minus速度边界条件*/
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			if (fabs(NcpMinus.x) < 1e-12)
			{
				velocity.y = ValMinus;
				PIStar = (velocity.y - cc*(na + ma*ValPlus) - dd*(nb + mb*ValPlus)) /
					(cc*ma1 + dd*mb1);
				velocity.x = aa*(na + ma1*PIStar + ma*ValPlus) +
					bb*(nb + mb1*PIStar + mb*ValPlus);
			}
			else if (fabs(NcpMinus.y) < 1e-12)
			{
				velocity.x = ValMinus;
				PIStar = (velocity.x - aa*(na + ma*ValPlus) - bb*(nb + mb*ValPlus)) /
					(aa*ma1 + bb*mb1);
				velocity.y = cc*(na + ma1*PIStar + ma*ValPlus) +
					dd*(nb + mb1*PIStar + mb*ValPlus);
			}
		}
		IsSolved = true;
	}

	/*Plus速度+Minus压力边界条件*/
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			if (fabs(NcpPlus.x) < 1e-12)
			{
				velocity.y = ValPlus;
				PIStar = (velocity.y - cc*(na + ma1*ValMinus) - dd*(nb + mb1*ValMinus)) /
					(cc*ma + dd*mb);
				velocity.x = aa*(na + ma*PIStar + ma1*ValMinus) +
					bb*(nb + mb*PIStar + mb1*ValMinus);
			}
			else if (fabs(NcpPlus.y) < 1e-12)
			{
				velocity.x = ValPlus;
				PIStar = (velocity.x - aa*(na + ma1*ValMinus) - bb*(nb + mb1*ValMinus)) /
					(aa*ma + bb*mb);
				velocity.y = cc*(na + ma*PIStar + ma1*ValMinus) +
					dd*(nb + mb*PIStar + mb1*ValMinus);
			}
		}
		IsSolved = true;
	}

	/*Plus对称轴+Minus对称轴边界条件*/
	if (enumPlus == SymmetryBoundary || enumMinus == SymmetryBoundary)
	{
		velocity.y = 0.;
		velocity.x = na / MatrixP.xx;
		IsSolved = true;
	}

	/*Plus对称轴+Minus速度边界条件*/
	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == VelocityBoundary || enumMinus == WallBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValMinus * NcpMinus.x;
			IsSolved = true;
		}
	}

	/*Plus速度+Minus对称轴边界条件*/
	if (enumPlus == VelocityBoundary || enumPlus == WallBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = ValPlus * NcpPlus.x;
			IsSolved = true;
		}
	}

	/*Plus对称轴+Minus压力边界条件*/
	if (enumPlus == SymmetryBoundary)
	{
		if (enumMinus == FreeBoundary || enumMinus == PressureBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValPlus*ma) / MatrixP.xx;
			IsSolved = true;
		}
	}

	/*Plus压力+Minus对称轴边界条件*/
	if (enumPlus == FreeBoundary || enumPlus == PressureBoundary)
	{
		if (enumMinus == SymmetryBoundary)
		{
			velocity.y = 0.;
			velocity.x = (na + ValMinus*ma1) / MatrixP.xx;
			IsSolved = true;
		}
	}

	if (!IsSolved)
	{
		printf("Boundary condition is not solved\n");
	}
	return velocity;
}

/*生成悬挂点的节点矩阵MatrixP和右端项RHS，以及TwoBouns[2]*/
static void 
quadrant_hanging_point_matrix_assemble_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside, *m_quad_data_full;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face, parent_face_index;
	CDoubleMatrix	MatrixP = CDoubleMatrix(0., 0., 0., 0.);
	CDoubleVector	RHS = CDoubleVector(0., 0.);
	CPointBounInfo	OneBounPlus, OneBounMinus, BounParent;

	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	/*two sides of the interface*/
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);

		if (side[i]->is_hanging == Hanging)
		{
			int i_parent = 1 - i;
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);

			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}

			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_full = (p4est_quadrant *)side[full_index]->is.full.quad;
			parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_quad_data_full = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_quad_data_full = (quad_data_t *)quad_full->p.user_data;
			}
			ParentBounInfo	*PCInfo = (ParentBounInfo	*)&m_quad_data_full->m_pc_edge_data;

			CDoubleVector	master_coord[2];//主点a和b的坐标

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				master_coord[0] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::LEFTUP];
				break;
			case quad_data_t::EnumEdge::RIGHT:
				master_coord[0] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_coord[1] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::RIGHTUP];
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				master_coord[0] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::RIGHTBOTTOM];
				break;
			case quad_data_t::EnumEdge::UP:
				master_coord[0] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::LEFTUP];
				master_coord[1] = m_quad_data_full->m_vara.VecCnData[idcnCoords_relaxed][quad_data_t::EnumCorner::RIGHTUP];
				break;
			}


			MatrixP = m_quad_data->m_vara.MarCnData[idcnMcp][m_which_corner[0]] +
				m_quad_data_aside->m_vara.MarCnData[idcnMcp][m_which_corner[1]] +
				m_quad_data_full->m_vara.MarCnData[ideMcp][parent_face_index];
			RHS = m_quad_data->m_vara.VecCnData[idcnRHS][m_which_corner[0]] +
				m_quad_data_aside->m_vara.VecCnData[idcnRHS][m_which_corner[1]] +
				m_quad_data_full->m_vara.VecCnData[ideRHS][parent_face_index];
			m_quad_data->points[m_which_corner[0]].MatrixP = MatrixP;
			m_quad_data->points[m_which_corner[0]].RHS = RHS;
			m_quad_data_aside->points[m_which_corner[1]].MatrixP = MatrixP;
			m_quad_data_aside->points[m_which_corner[1]].RHS = RHS;

			CDoubleVector		hanging_coord;
			hanging_coord = m_quad_data->m_vara.VecCnData[idcnCoords_half][m_which_corner[0]];

			OneBounPlus.Ncp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Ncp;
			OneBounPlus.Lcp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Lcp;
			OneBounPlus.delta_u_cp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].delta_u_cp;
			OneBounPlus.Uc_cur = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Uc_cur;
			OneBounPlus.Zc = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Zcp;
			OneBounPlus.enumType = WallBoundary;
			OneBounMinus.Ncp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Ncp;
			OneBounMinus.Lcp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Lcp;
			OneBounMinus.delta_u_cp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].delta_u_cp;
			OneBounMinus.Uc_cur = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Uc_cur;
			OneBounMinus.Zc = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Zcp;
			OneBounMinus.enumType = WallBoundary;
			BounParent.Ncp = --OneBounPlus.Ncp;
			BounParent.Lcp = OneBounPlus.Lcp + OneBounMinus.Lcp;
			BounParent.Uc_cur = m_quad_data_full->m_vara.VecCnData[idReconstructVelocity][0];
			BounParent.delta_u_cp = OneBounPlus.delta_u_cp + OneBounPlus.Uc_cur - BounParent.Uc_cur;
			BounParent.Zc = m_quad_data_full->m_cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::plus].Zcp;

			BounParent.enumType = WallBoundary;

			m_quad_data->points[m_which_corner[0]].IsHanging = true;
			m_quad_data->points[m_which_corner[0]].TwoBouns[0] = OneBounPlus;
			m_quad_data->points[m_which_corner[0]].TwoBouns[1] = OneBounMinus;
			m_quad_data->points[m_which_corner[0]].BounParent = BounParent;
			m_quad_data->points[m_which_corner[0]].master_coord_relaxed[0] = master_coord[0];
			m_quad_data->points[m_which_corner[0]].master_coord_relaxed[1] = master_coord[1];
			m_quad_data->points[m_which_corner[0]].hanging_coord = hanging_coord;

			m_quad_data_aside->points[m_which_corner[1]].IsHanging = true;
			m_quad_data_aside->points[m_which_corner[1]].TwoBouns[0] = OneBounMinus;
			m_quad_data_aside->points[m_which_corner[1]].TwoBouns[1] = OneBounPlus;
			m_quad_data_aside->points[m_which_corner[1]].BounParent = BounParent;
			m_quad_data_aside->points[m_which_corner[1]].master_coord_relaxed[0] = master_coord[0];
			m_quad_data_aside->points[m_which_corner[1]].master_coord_relaxed[1] = master_coord[1];
			m_quad_data_aside->points[m_which_corner[1]].hanging_coord = hanging_coord;
		}
	}
}

void MatrixAssemble(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	my_user_data_t *m_user_data = (my_user_data_t *)malloc(sizeof(my_user_data_t));
	m_user_data->p4est_data = (void *)p4est_data;
	m_user_data->quad_data = (void *)ghost_data;

	/*1, 网格隅角矩阵Mcp,Mcp*Up等*/
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_corner_matrix_assemble_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

/*2, 对隅角矩阵进行累加，装配节点矩阵，得到方程左右端MatrixP和RHS：MatrixP * Up = RHS  */
	p4est_iterate(p4est,
		ghost,
		(void*)m_user_data,
		NULL,
		NULL,  
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		quadrant_corner_to_point_matrix_assemble_callback);        /* 非悬点的矩阵装配 */
}

static void quadrant_copy_velocity_from_lag_to_relax_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	
	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->VecCnData[idcnVelocity_relaxed][k] = m_vara->VecCnData[idcnVelocity_lag][k];
	}
}

static void quadrant_update_parent_velo_press_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside, *m_quad_data_full;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face, parent_face_index;

	/*the face index defined by p4est*/
	//--------------------
	//|         3         |
	//|                   |
	//|0                 1|
	//|                   |
	//|         2         |
	//--------------------

	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	/*two sides of te interface*/
	for (int i = 0; i < 2; i++)
	{
		if (sides->elem_count < 2 && i == 1)
		{
			continue;
		}
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		
		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_full = (p4est_quadrant *)side[full_index]->is.full.quad;
			parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_quad_data_full = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_quad_data_full = (quad_data_t *)quad_full->p.user_data;
			}
			ParentBounInfo	*PCInfo = (ParentBounInfo	*)&m_quad_data_full->m_pc_edge_data;
			PCInfo[parent_face_index].Hanging_velocity = m_quad_data->m_vara.VecCnData[idcnVelocity_lag][m_which_corner[0]];

			if (m_quad_data->points[m_which_corner[0]].IsHanging == true &&
				m_quad_data->points[m_which_corner[0]].add_dissipation_parent == true)
			{
				PCInfo[parent_face_index].addDiss = true;
				PCInfo[parent_face_index].ParentPIStar = m_quad_data->points[m_which_corner[0]].pi_constrained_parent;
			}
		}
	}
}

/*根据松弛约束条件下,master节点的速度和位置，更新悬点速度*/
void ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	/*1. 更新松弛约束所需要的master节点速度，位置，以及其他几何量*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		quadrant_compute_relaxed_info_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

	/*2.计算悬点处父网格矩阵*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		quadrant_parent_edge_matrix_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

	/*3.装配悬点矩阵*/
	p4est_iterate(p4est,
		ghost,          // 无需ghost层数据
		(void*) ghost_data,   // 用户自定义数据，无需输出
		NULL, // 更新回调函数
		quadrant_hanging_point_matrix_assemble_callback,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

	/*4.求解悬点父网格边界压力*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		NULL, // 更新回调函数
		quadrant_relaxed_hanging_solver_callback,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*隅角速度求解的回调函数*/
static void quadrant_corner_velocity_callback(p4est_iter_corner_info_t *info, void *user_data)
{
	p4est_iter_corner_side_t	*side[CNDIM];  //每个隅角有CNDIM个sides
	sc_array_t					*sides = &(info->sides);
	int							which_corner, cnid, is_ghost, m_size;
	int							quadid;
	int							tree_boundary;
	bool						is_boundary /*是否是边界点*//*, is_hanging  是否是悬挂点 */;
	quad_data_t					*m_data;
	CVariable					*m_vara;
	quad_data_t					*ghost_data = (quad_data_t *)user_data;

	/*判断当前corner是否是边界，判据标识为is_boundary*/
	tree_boundary = info->tree_boundary;
	//is_boundary = false;
	//if (tree_boundary == P4EST_CONNECT_CORNER || tree_boundary == P4EST_CONNECT_FACE) { is_boundary = true; }

	m_size = int(sides->elem_count);
	CHalf_edge_data *m_plus, *m_minus;
	is_boundary = false;
	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t *)&ghost_data[quadid];
		}
		else
		{
			m_data = (quad_data_t *)side[i]->quad->p.user_data;
		}
		m_plus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&m_data->m_cndata[cnid].hdata[CHalf_edge_data::cside::minus];
		if (m_plus->enumBYD != InnerBoundary) { is_boundary = true; }
		if (m_minus->enumBYD != InnerBoundary) { is_boundary = true; }
	}

	for (int i = 0; i < m_size; i++)
	{
		side[i] = p4est_iter_cside_array_index_int(sides, i);
		quadid = side[i]->quadid;
		which_corner = side[i]->corner;
		cnid = convert_which_corner_to_user_define_index(which_corner);

		is_ghost = side[i]->is_ghost;
		if (is_ghost)
		{
			m_data = (quad_data_t *)&ghost_data[quadid];
			m_vara = (CVariable *)&ghost_data[quadid].m_vara;
		}
		else
		{
			m_data = (quad_data_t *)side[i]->quad->p.user_data;
			m_vara = (CVariable *)&m_data->m_vara;
		}


		if (is_boundary)		/*边界求解器，一个边界节点必须对应两个边界边，A和B*/
		{




			m_data->points[cnid].velo_lag = BoundaryNodeVelocityComputation(
				m_data->points[cnid].TwoBouns[0],    /*边界A*/
				m_data->points[cnid].TwoBouns[1],    /*边界B*/
				m_data->points[cnid].MatrixP,  /*节点矩阵MatrixP*/
				m_data->points[cnid].RHS);     /*节点右端项RHS*/
		}
		else                /*一般求解器*/
		{
			CDoubleMatrix MatrixP_Inverse;
			MatrixP_Inverse = GeometryAlg::MatrixInverse(m_data->points[cnid].MatrixP);
			m_data->points[cnid].velo_lag = GeometryAlg::MatrixDotVector(MatrixP_Inverse, m_data->points[cnid].RHS);
		}
		/*小于m_eps时置零*/
		if (fabs(m_data->points[cnid].velo_lag.x) < m_eps) { m_data->points[cnid].velo_lag.x = 0.; }
		if (fabs(m_data->points[cnid].velo_lag.y) < m_eps) { m_data->points[cnid].velo_lag.y = 0.; }

		//cndata[cnid].node_velo_lag
		m_vara->VecCnData[idcnVelocity_lag][cnid] = m_data->points[cnid].velo_lag;





	}
}

/*更新隅角速度*/
static void ComputeCornerNodeVelocity(p4est_t * p4est, p4est_ghost_t * ghost, void *ghost_data)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	my_user_data_t *m_user_data = (my_user_data_t *)malloc(sizeof(my_user_data_t));
	m_user_data->p4est_data = (void *)p4est_data;
	m_user_data->quad_data = (void *)ghost_data;

	p4est_iterate(p4est,
		ghost,          // 无需ghost层数据
		(void*)m_user_data,   // 用户自定义数据，无需输出
		NULL, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		quadrant_corner_velocity_callback);   

	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		quadrant_copy_velocity_from_lag_to_relax_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新坐标回调函数*/
static void quadrant_update_corner_coordinate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	double				delta_time = p4est_data->dt_iter;
	for (int k = 0; k < CNDIM; k++)  /*四个角点循环*/
	{
		m_vara->VecCnData[idcnCoords_lag][k] = m_vara->VecCnData[idcnCoords_half][k] +
			CDoubleVector(m_vara->VecCnData[idcnVelocity_lag][k].x * delta_time, m_vara->VecCnData[idcnVelocity_lag][k].y * delta_time);
	}
	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->VecCnData[idcnCoords_lag][i]; }

	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	m_vara->VecCData[idCentroidCoord_lag] = center_point;

	for (int idIndex = idEChildrenCoordinate_lag; idIndex < idVectorEdgeVariableNum; idIndex++)
	{
		int idcnVara;
		switch (idIndex)
		{
		case idEChildrenCoordinate_lag:
			idcnVara = idcnCoords_lag;
			break;
		case idEChildrenVelocity_lag:
			idcnVara = idcnVelocity_lag;
			break;
		default:
			break;
		}
		m_vara->VecEdata[idIndex][quad_data_t::EnumEdge::LEFT] = 0.5 *
			(m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::LEFTUP] +
				m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::LEFTBOTTOM]);
		m_vara->VecEdata[idIndex][quad_data_t::EnumEdge::RIGHT] = 0.5 *
			(m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::RIGHTUP] +
				m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::RIGHTBOTTOM]);
		m_vara->VecEdata[idIndex][quad_data_t::EnumEdge::BOTTOM] = 0.5 *
			(m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::LEFTBOTTOM] +
				m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::RIGHTBOTTOM]);
		m_vara->VecEdata[idIndex][quad_data_t::EnumEdge::UP] = 0.5 *
			(m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::LEFTUP] +
				m_vara->VecCnData[idcnVara][quad_data_t::EnumCorner::RIGHTUP]);
	}
}

/*计算节点坐标*/
static void ComputeCoordinate(p4est_t * p4est)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;
	/*1. 更新隅角坐标*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_update_corner_coordinate_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新网格密度的回调函数*/
static void quadrant_update_density_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	CDoubleVector		m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->VecCnData[idcnCoords_lag][i]; }

	m_vara->DouCData[idVolume] = GeometryAlg::CalculateCellVolume(coordinate_type, m_cell_coord);
	m_vara->DouCData[idDensity_lag] = m_vara->DouCData[idMass] / m_vara->DouCData[idVolume];







}

/*计算网格密度*/
static void UpdateDensity(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_update_density_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新动量方程的回调函数*/
static void quadrant_update_momentum_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	int					scheme_type = p4est_data->Scheme_type;
	CDoubleVector		SumFcp = CDoubleVector(0., 0.);/*隅角力之和*/
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
			SumFcp += m_vara->VecCnData[idcnFcp][cnid] + m_vara->VecCnData[idcnFluxRelaxed][cnid];
		}
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
		{
			if (PCInfo[eind].IsParentChildBoun==true)
			{
				SumFcp += m_vara->VecCnData[ideFcp][eind] + PCInfo[eind].FluxRelaxed;
			}
		}
	}

	if (scheme_type == p4est_data_t::MySchemeType::ControlVolume)
	{
		m_vara->VecCData[idCentroidVelo_lag] = m_vara->VecCData[idCentroidVelo_half] -
			p4est_data->dt_iter * SumFcp / m_vara->DouCData[idMass];
	}
	else if (scheme_type == p4est_data_t::MySchemeType::AreaWeighted)/*面格式*/
	{
		CDoubleVector m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->VecCnData[idcnCoords_cur][i]; }
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		m_vara->VecCData[idCentroidVelo_lag] = m_vara->VecCData[idCentroidVelo_half] -
			p4est_data->dt_iter * SumFcp / m_vara->DouCData[idMass] / center_point.x;
	}
}

/*更新动量方程*/
static void UpdateMomentumEquation(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_update_momentum_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新压力做功回调函数*/
static void quadrant_compute_work_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					coordinate_type = p4est_data->coord_type;
	double				m_alpha = 1.;
	double				m_beta = 1.;

	//cdata->KineticVariation = 0.;
	//cdata->TotalWork = 0.;
	m_vara->DouCData[idKineticVariation] = 0.;
	m_vara->DouCData[idTotalWork] = 0.;

	if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
	{
		m_alpha = 2.* M_PI;
		m_beta = 2. * M_PI * m_vara->VecCData[idCentroidCoord_cur].y;
	}
	CDoubleVector Velo = 0.5 * (m_vara->VecCData[idCentroidVelo_half] + m_vara->VecCData[idCentroidVelo_lag]);
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		/*根据动量方程计算动能变化*/
		if (coordinate_type == p4est_data_t::MyCoordType::plane)
		{
			//cdata->KineticVariation
			m_vara->DouCData[idKineticVariation] += m_beta * 
				Velo^ (m_vara->VecCnData[idcnFcp][cnid]+ m_vara->VecCnData[idcnFluxRelaxed][cnid]);
		}
		if (coordinate_type == p4est_data_t::MyCoordType::cylinder)
		{
			//cdata->KineticVariation
			m_vara->DouCData[idKineticVariation] += m_beta * Velo^ m_vara->VecCnData[idAWFcp][cnid];
		}

		/*根据总能方程计算总能的变化*/
		m_vara->DouCData[idTotalWork] += m_alpha*
			m_vara->VecCnData[idcnVelocity_lag][cnid] ^ 
			(m_vara->VecCnData[idcnFcp][cnid]+ m_vara->VecCnData[idcnFluxRelaxed][cnid]); 
	}

	for (int eind = 0; eind < CNDIM; eind++)
	{
		/*父子网格边界*/
		if (PCInfo[eind].IsParentChildBoun==true)
		{
			if (coordinate_type == p4est_data_t::MyCoordType::plane)
			{
				m_vara->DouCData[idKineticVariation] += m_beta * Velo ^
					(m_vara->VecCnData[ideFcp][eind] + PCInfo[eind].FluxRelaxed);
			}
			m_vara->DouCData[idTotalWork] += m_alpha* PCInfo[eind].Hanging_velocity ^
				(m_vara->VecCnData[ideFcp][eind] + PCInfo[eind].FluxRelaxed);
		}
	}
}

/*更新压力做功*/
static void ComputeWork(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_compute_work_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新总能和内能方程的回调函数*/
static void quadrant_update_energy_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t			*data = (quad_data_t *)info->quad->p.user_data;
	CVariable			*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	/*二阶龙格库塔法更新总能*/
	m_vara->DouCData[idTotalEnergy_lag] = m_vara->DouCData[idTotalEnergy_half] - p4est_data->dt_iter * m_vara->DouCData[idTotalWork] / m_vara->DouCData[idMass];


	double source = 0.;
	if (p4est_data->which_case == ProblemNo::TaylorGreen)
	{
		source = p4est_data->dt_iter * 5.*M_PI / 8.*m_vara->DouCData[idVolume] *
			(cos(3.*M_PI*m_vara->VecCData[idCentroidCoord_lag].x)*cos(M_PI * m_vara->VecCData[idCentroidCoord_lag].y) -
				cos(M_PI*m_vara->VecCData[idCentroidCoord_lag].x)*cos(3.*M_PI*m_vara->VecCData[idCentroidCoord_lag].y)) / m_vara->DouCData[idMass];
	}

	if (m_vara->DouCData[idTotalEnergy_lag] > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}

	/*根据总能守恒原则更新内能*/
	m_vara->DouCData[idInternalEnergy_lag] = m_vara->DouCData[idInternalEnergy_half] - p4est_data->dt_iter
		* (m_vara->DouCData[idTotalWork] - m_vara->DouCData[idKineticVariation]) / m_vara->DouCData[idMass];
	m_vara->DouCData[idInternalEnergy_lag] += source;
	if (m_vara->DouCData[idInternalEnergy_lag] > m_eps)
	{
	}
	else
	{

		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}
}

/*更新总能方程*/
static void UpdateEnergyEquation(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_update_energy_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*更新状态方程的回调函数*/
static void quadrant_update_EOS_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t *data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	m_vara->DouCData[idPressure_lag] = PhysicalAlg::EquationOfState(m_vara->DouCData[idGamma], m_vara->DouCData[idDensity_lag], m_vara->DouCData[idInternalEnergy_lag]);
	if (m_vara->DouCData[idPressure_lag] > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the value of pressure is illegal\n");
	}
}

/*更新状态方程*/
static void UpdateEquationOfState(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_update_EOS_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*接收网格中心量数值解的回调函数*/
static void quadrant_accept_center_solution_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	m_vara->VecCData[idCentroidCoord_cur] = m_vara->VecCData[idCentroidCoord_lag];
	m_vara->VecCData[idCentroidVelo_cur] = m_vara->VecCData[idCentroidVelo_lag];
	m_vara->DouCData[idDensity_cur] = m_vara->DouCData[idDensity_lag];
	m_vara->DouCData[idTotalEnergy_cur] = m_vara->DouCData[idTotalEnergy_lag];
	m_vara->DouCData[idInternalEnergy_cur] = m_vara->DouCData[idInternalEnergy_lag];

	if (m_vara->DouCData[idInternalEnergy_cur] > m_eps)
	{
	}
	else
	{
		P4EST_GLOBAL_PRODUCTIONF("the total energy of quad %d is negative!\n", info->quadid);
		std::abort();
	}

	m_vara->DouCData[idPressure_cur] = m_vara->DouCData[idPressure_lag];

	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_vara->VecCnData[idcnCoords_cur][cnid] = m_vara->VecCnData[idcnCoords_lag][cnid];
		m_vara->VecCnData[idcnVelocity_cur][cnid] = m_vara->VecCnData[idcnVelocity_lag][cnid];
	}

	generate_children_info_from_parent(p4est_data, m_vara);
}









































































































































































































static void quadrant_total_energy_error_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;

	p4est_data->total_energy_lag += m_vara->DouCData[idMass] * m_vara->DouCData[idTotalEnergy_lag];
	p4est_data->total_energy_cur += m_vara->DouCData[idMass] * m_vara->DouCData[idTotalEnergy_cur];


}

static void
quadrant_reset_parent_edge_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*m_quad_data = (quad_data_t *)info->quad->p.user_data;

	for (int eind = 0; eind < CNDIM; eind++)
	{
		m_quad_data->m_pc_edge_data[eind].IsParentChildBoun = false;
		m_quad_data->m_pc_edge_data[eind].Lcp[0] = 0.;
		m_quad_data->m_pc_edge_data[eind].Lcp[1] = 0.;
		m_quad_data->m_pc_edge_data[eind].Ncp[0] = CDoubleVector(0., 0.);
		m_quad_data->m_pc_edge_data[eind].Ncp[1] = CDoubleVector(0., 0.);
		m_quad_data->m_pc_edge_data[eind].FluxRelaxed = CDoubleVector(0., 0.);
	}
}

static void quadrant_set_init_parent_edge_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	p4est_data_t	*p4est_data = (p4est_data_t *)user_data;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_child1_data, *m_child2_data, *m_parent_data;/*悬点一侧的children网格数据和另一侧parent网格数据*/
	CVariable		*m_child1_vara, *m_child2_vara;
	CCorner_data	*m_child1_cndata, *m_child2_cndata;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];
	/*two sides of the interface*/

	P4EST_ASSERT(sides->elem_count == 2);
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);
		if (side[i]->is_hanging == Hanging)/*这条边有悬点*/
		{
			p4est_quadrant	*quad_child1 = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad_child1->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx_child1 = quad_child1->x;
			p4est_qcoord_t qy_child1 = quad_child1->y;
			p4est_quadrant	*quad_child2 = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_child2 = quad_child2->x;
			p4est_qcoord_t qy_child2 = quad_child2->y;
			which_face = side[i]->face;
			get_hanging_edge_info_from_logical_position(which_face, qx_child1, qy_child1,
				qx_child2, qy_child2, length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);
			if (side[i]->is.hanging.is_ghost[0])
			{
				m_child1_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
				m_child1_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[0]].m_vara;
				m_child1_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[0]].m_cndata);
			}
			else
			{
				m_child1_data = (quad_data_t *)quad_child1->p.user_data;
				m_child1_vara = (CVariable *)&m_child1_data->m_vara;
				m_child1_cndata = (CCorner_data *)&(m_child1_data->m_cndata);
			}
			if (side[i]->is.hanging.is_ghost[1])
			{
				m_child2_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
				m_child2_vara = (CVariable *)&ghost_data[side[i]->is.hanging.quadid[1]].m_vara;
				m_child2_cndata = (CCorner_data *)&(ghost_data[side[i]->is.hanging.quadid[1]].m_cndata);
			}
			else
			{
				m_child2_data = (quad_data_t *)quad_child2->p.user_data;
				m_child2_vara = (CVariable *)&m_child2_data->m_vara;
				m_child2_cndata = (CCorner_data *)&(m_child2_data->m_cndata);
			}

			int full_index = GeometryAlg::GetCircleNext(2, i);
			side[full_index] = p4est_iter_fside_array_index_int(sides, full_index);
			p4est_quadrant	*quad_parent = (p4est_quadrant	*)side[full_index]->is.full.quad;
			int parent_face_index = side[GeometryAlg::GetCircleNext(2, i)]->face;
			if (side[full_index]->is.full.is_ghost)
			{
				m_parent_data = (quad_data_t *)&ghost_data[side[full_index]->is.full.quadid];
			}
			else
			{
				m_parent_data = (quad_data_t *)quad_parent->p.user_data;
			}
			CCorner_data		*cndata = (CCorner_data *)&m_parent_data->m_cndata;
			ParentBounInfo		*PCInfo = (ParentBounInfo  *)&m_parent_data->m_pc_edge_data;
			CHalf_edge_data *m_plus, *m_minus;

			PCInfo[parent_face_index].IsParentChildBoun =
				m_child1_data->points[m_which_corner[0]].IsHanging;

			PCInfo[parent_face_index].ParentPIStar =

				m_child1_data->points[m_which_corner[0]].pi_constrained_parent;

			PCInfo[parent_face_index].Hanging_velocity =
				m_child1_data->m_vara.VecCnData[idcnVelocity_lag][m_which_corner[0]];

			PCInfo[parent_face_index].Lcp[0] =
				m_child1_data->points[m_which_corner[0]].TwoBouns[0].Lcp;

			PCInfo[parent_face_index].Lcp[1] =
				m_child2_data->points[m_which_corner[1]].TwoBouns[0].Lcp;

			PCInfo[parent_face_index].Ncp[0] = --m_child1_data->points[m_which_corner[0]].TwoBouns[0].Ncp;
			PCInfo[parent_face_index].Ncp[1] = --m_child2_data->points[m_which_corner[1]].TwoBouns[0].Ncp;

			CDoubleVector	master_coord[2], hanging_coord;
			hanging_coord = m_child1_data->m_vara.VecCnData[idcnCoords_cur][m_which_corner[0]];

			switch (parent_face_index)
			{
			case quad_data_t::EnumEdge::LEFT:
				m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTBOTTOM].hdata[CHalf_edge_data::cside::plus];
				m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::minus];
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];
				m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
				m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
				break;
			case quad_data_t::EnumEdge::RIGHT:
				m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTUP].hdata[CHalf_edge_data::cside::plus];
				m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTBOTTOM].hdata[CHalf_edge_data::cside::minus];
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];
				m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
				m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
				break;
			case quad_data_t::EnumEdge::BOTTOM:
				m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTBOTTOM].hdata[CHalf_edge_data::cside::plus];
				m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTBOTTOM].hdata[CHalf_edge_data::cside::minus];
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTBOTTOM];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTBOTTOM];
				m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
				m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
				break;
			case quad_data_t::EnumEdge::UP:
				m_plus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::LEFTUP].hdata[CHalf_edge_data::cside::plus];
				m_minus = (CHalf_edge_data *)&cndata[quad_data_t::EnumCorner::RIGHTUP].hdata[CHalf_edge_data::cside::minus];
				master_coord[0] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::RIGHTUP];
				master_coord[1] = m_parent_data->m_vara.VecCnData[idcnCoords_cur][quad_data_t::EnumCorner::LEFTUP];
				m_plus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[1], hanging_coord) / 2.;
				m_minus->Lcp = GeometryAlg::GetPointToPointDistance(master_coord[0], hanging_coord) / 2.;
				break;
			default:
				break;
			}
		}
	}
}

static void
quadrant_get_children_hanging_info_callback(p4est_iter_face_info_t *info, void *user_data)
{
	p4est_t			*p4est = info->p4est;
	quad_data_t		*ghost_data = (quad_data_t *)user_data;
	quad_data_t		*m_quad_data, *m_quad_data_aside;
	p4est_iter_face_side_t *side[2];
	sc_array_t				*sides = &(info->sides);
	int				which_face;
	CPointBounInfo	OneBounPlus, OneBounMinus;

	int				m_which_corner[2], m_master_corner[2], m_unconstrained_master_corner[2], m_which_side[2];

	/*two sides of the interface*/
	if (sides->elem_count != 2) { return; }
	for (int i = 0; i < 2; i++)
	{
		side[i] = p4est_iter_fside_array_index_int(sides, i);

		if (side[i]->is_hanging == Hanging)
		{
			p4est_quadrant	*quad = side[i]->is.hanging.quad[0];
			if (side[i]->is.hanging.quadid[0]<0
				|| side[i]->is.hanging.quadid[1]<0
				|| side[i]->is.hanging.quadid[0]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[1]>info->p4est->global_num_quadrants
				|| side[i]->is.hanging.quadid[0] == side[i]->is.hanging.quadid[1]) {
				continue;
			}
			int			level = quad->level;
			p4est_qcoord_t length = P4EST_QUADRANT_LEN(level);
			p4est_qcoord_t qx = quad->x;
			p4est_qcoord_t qy = quad->y;
			p4est_quadrant	*quad_aside = side[i]->is.hanging.quad[1];
			p4est_qcoord_t qx_aside = quad_aside->x;
			p4est_qcoord_t qy_aside = quad_aside->y;
			which_face = side[i]->face;

			get_hanging_edge_info_from_logical_position(which_face, qx, qy, qx_aside, qy_aside,
				length, m_which_corner, m_which_side, m_master_corner, m_unconstrained_master_corner);

			if (side[i]->is.hanging.is_ghost[0])
			{
				m_quad_data = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[0]];
			}
			else
			{
				m_quad_data = (quad_data_t *)quad->p.user_data;
			}

			if (side[i]->is.hanging.is_ghost[1])
			{
				m_quad_data_aside = (quad_data_t *)&ghost_data[side[i]->is.hanging.quadid[1]];
			}
			else
			{
				m_quad_data_aside = (quad_data_t *)quad_aside->p.user_data;
			}

			OneBounPlus.Ncp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Ncp;
			OneBounPlus.Lcp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Lcp;
			OneBounPlus.delta_u_cp = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].delta_u_cp;
			OneBounPlus.Uc_cur = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Uc_cur;
			OneBounPlus.Zc = m_quad_data->m_cndata[m_which_corner[0]].hdata[m_which_side[0]].Zcp;
			OneBounPlus.enumType = WallBoundary;
			OneBounMinus.Ncp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Ncp;
			OneBounMinus.Lcp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Lcp;
			OneBounMinus.delta_u_cp = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].delta_u_cp;
			OneBounMinus.Uc_cur = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Uc_cur;
			OneBounMinus.Zc = m_quad_data_aside->m_cndata[m_which_corner[1]].hdata[m_which_side[1]].Zcp;
			OneBounMinus.enumType = WallBoundary;

			m_quad_data->points[m_which_corner[0]].IsHanging = true;
			m_quad_data->points[m_which_corner[0]].TwoBouns[0] = OneBounPlus;
			m_quad_data->points[m_which_corner[0]].TwoBouns[1] = OneBounMinus;

			m_quad_data_aside->points[m_which_corner[1]].IsHanging = true;
			m_quad_data_aside->points[m_which_corner[1]].TwoBouns[0] = OneBounMinus;
			m_quad_data_aside->points[m_which_corner[1]].TwoBouns[1] = OneBounPlus;
		}
	}

}

static void Get_AMR_BDY_info(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		NULL, // 更新回调函数
		quadrant_get_children_hanging_info_callback,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

	/*隅角悬挂信息默认设为false*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		quadrant_reset_parent_edge_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据

	/*根据face悬挂信息，决定隅角悬挂信息*/
	p4est_iterate(p4est,
		ghost,          // 无需ghost层数据
		(void*)ghost_data,   // 用户自定义数据，无需输出
		NULL, // 更新回调函数
		quadrant_set_init_parent_edge_callback,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*接收数值解*/
static void AcceptNumericalSolution(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_accept_center_solution_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*统计总能误差*/
static void StatTotalEnergyError(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_data->total_energy_cur = 0.;
	p4est_data->total_energy_lag = 0.;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_total_energy_error_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
	if (p4est_data->current_step == 1)
	{
		p4est_data->total_energy_init = p4est_data->total_energy_cur;
	}

	p4est_data->EnergyFile << blank << blank << p4est_data->current_time << blank << blank <<
		(p4est_data->total_energy_lag - p4est_data->total_energy_cur) /
		p4est_data->total_energy_cur << endl;

	P4EST_GLOBAL_PRODUCTIONF("the total energy error is %#.16g\n", (p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init);
	if (abs((p4est_data->total_energy_lag - p4est_data->total_energy_init) /
		p4est_data->total_energy_init) > 1e-8)
	{
		P4EST_GLOBAL_PRODUCTIONF("The total energy is not conservative after time step\n");
		abort();
	}
}

static void quadrant_compute_corner_force_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	/* 1. 获取当前quadrant的用户数据指针 */
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	ParentBounInfo		*PCInfo = (ParentBounInfo  *)&data->m_pc_edge_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	int					Scheme_type = p4est_data->Scheme_type;
	CCorner_data		*cndata = (CCorner_data *)&data->m_cndata;
	CHalf_edge_data		*m_plus, *m_minus;
	

	for (int k = 0; k < CNDIM; k++)
	{
		CDoubleVector DeltaU[CNDIM], McpDeltaUc, AWMcpDeltaUc, LcpNcpPc;
		m_plus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::plus];
		m_minus = (CHalf_edge_data *)&cndata[k].hdata[CHalf_edge_data::cside::minus];
		m_vara->VecCnData[idReconstructVelocity][k] = m_vara->VecCData[idCentroidVelo_cur];
		m_vara->DouCnData[idReconstructDensity][k] = m_vara->DouCData[idDensity_cur];
		m_vara->DouCnData[idReconstructPressure][k] = m_vara->DouCData[idPressure_cur];
		DeltaU[k] = m_vara->VecCnData[idcnVelocity_lag][k] - m_vara->VecCnData[idReconstructVelocity][k];
		LcpNcpPc = (m_plus->Rcp * m_plus->Lcp * m_plus->Ncp +
			m_minus->Rcp * m_minus->Lcp * m_minus->Ncp)*m_vara->DouCnData[idReconstructPressure][k];
		McpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnMcp][k],DeltaU[k]);
		m_vara->VecCnData[idcnFcp][k] = (m_plus->Rcp * m_plus->Lcp * m_plus->Ncp +
			m_minus->Rcp * m_minus->Lcp * m_minus->Ncp)*
			m_vara->DouCnData[idReconstructPressure][k] - McpDeltaUc;
		if (Scheme_type == p4est_data_t::MySchemeType::AreaWeighted)
		{
			AWMcpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[idcnAWMcp][k],
				DeltaU[k]);
			m_vara->VecCnData[idAWFcp][k] = (m_plus->Lcp * m_plus->Ncp + m_minus->Lcp * m_minus->Ncp)*
				m_vara->DouCnData[idReconstructPressure][k] - AWMcpDeltaUc;
		}
	}


	for (int eind = 0; eind < CNDIM; eind++)
	{
		CDoubleVector DeltaU, McpDeltaUc, LcpNcpPc;
		DeltaU = PCInfo[eind].Hanging_velocity
			- m_vara->VecCnData[idReconstructVelocity][eind];
		LcpNcpPc = (PCInfo[eind].Lcp[0] * PCInfo[eind].Ncp[0] + PCInfo[eind].Lcp[1] * PCInfo[eind].Ncp[1])*
			m_vara->DouCnData[idReconstructPressure][eind];
		McpDeltaUc = GeometryAlg::MatrixDotVector(m_vara->MarCnData[ideMcp][eind], DeltaU);
		m_vara->VecCnData[ideFcp][eind] = (PCInfo[eind].Lcp[0] * PCInfo[eind].Ncp[0] + PCInfo[eind].Lcp[1] * PCInfo[eind].Ncp[1])*
			m_vara->DouCnData[idReconstructPressure][eind] - McpDeltaUc;
	}
}

/*计算隅角力*/
static void ComputeCornerAndEdgeForce(p4est_t * p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		(void*)p4est_data,   // 用户自定义数据，无需输出
		quadrant_compute_corner_force_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

static void quadrant_flux_relaxed_reset_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	quad_data_t		*data = (quad_data_t *)(info->quad->p.user_data);
	CVariable				*m_vara = (CVariable *)&data->m_vara;
	CCorner_data		*cndata = (CCorner_data *)&data->m_cndata;



	for (int k = 0; k < CNDIM; k++)
	{
		m_vara->VecCnData[idcnFluxRelaxed][k] = CDoubleVector(0.,0.);
	}
}

void FluxRelaxedResetZero(p4est_t *p4est)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	/*1.网格主点隅角矩阵Mcp, Mcp*Up等*/
	p4est_iterate(p4est,
		NULL,          // 无需ghost层数据
		NULL,   // 用户自定义数据，无需输出
		quadrant_flux_relaxed_reset_callback, // 更新回调函数
		NULL,
#ifdef P4_TO_P8
		NULL,                  /* there is no callback for the
							   edges between quadrants */
#endif
		NULL);         // 无需额外数据
}

/*节点求解器*/
static void RiemannSolver(p4est_t * p4est, p4est_ghost_t * ghost, void *ghost_data)
{
	/*每一个时间步，松弛约束力归零*/
	FluxRelaxedResetZero(p4est);
	/*fixed iteration method。显式格式，用简单迭代法求解节点速度*/
	for (int iter_num = 0; iter_num < fixed_iter_num; iter_num++)
	{
		/*1. 装配网格矩阵MatrixCP，累加得到节点矩阵MatrixP，并获得Matrix * Up = RHS的右端项RHS*/
		MatrixAssemble(p4est, ghost, ghost_data);
		p4est_ghost_exchange_data(p4est, ghost, ghost_data);

		/*2. 根据Up = RHS * Matrix^-1，计算节点速度*/
		ComputeCornerNodeVelocity(p4est, ghost, ghost_data);
		p4est_ghost_exchange_data(p4est, ghost, ghost_data);

		/*3.更新两端master节点的速度，在参考坐标系中，以滑移约束条件，更新悬点速度*/
		ComputeHangingNodeVelocityUsingConstrainedConditionByMasterNodes(p4est, ghost, ghost_data);

		/*4. synchronize the ghost data after every iteration*/
		p4est_ghost_exchange_data(p4est, ghost, ghost_data);
	}

	/*3. 更新隅角力Fcp*/
	ComputeCornerAndEdgeForce(p4est);
}

/*二阶Runge_Kutta时间推进*/
static void two_stage_Runge_Kutta(p4est_t * p4est, p4est_ghost_t * ghost, void *ghost_data)
{
	p4est_data_t	*p4est_data = (p4est_data_t *)p4est->user_pointer;

	/*叶子网格边界条件*/
	get_boundary_from_p4est(p4est);

	for (size_t iter_num = 0; iter_num < 1; iter_num++)
	{
		switch (iter_num)
		{
		case 0:
			p4est_data->dt_iter = p4est_data->delta_time;
			break;
		case 1:
			p4est_data->dt_iter = 0.5 * p4est_data->delta_time;
			break;
		}

		/*1. 计算时间半步物理量*/
		CalculateHalfTimeVariable(p4est);

		/*2. 计算当前迭代步Rcp, Lcp, Ncp等几何量*/
		CalculateCornerRcpLcpNcp(p4est);

		/*确定AMR边界信息*/
		Get_AMR_BDY_info(p4est,ghost,ghost_data);
		p4est_ghost_exchange_data(p4est, ghost, ghost_data);

		/*3. 节点求解器，计算节点速度*/
		if (p4est_data->coord_type == p4est_data_t::RiemannSolver::GridAligned)
		{
			RiemannSolver(p4est, ghost, ghost_data);
		}

		/*4. 更新散度*/
		ComputeDivergence(p4est);

		/*5. 更新网格运动*/
		ComputeCoordinate(p4est);

		/*6. 更新密度*/
		UpdateDensity(p4est);

		/*7. 更新动量方程*/
		UpdateMomentumEquation(p4est);

		/*8. 更新压力做功*/
		ComputeWork(p4est);

		/*9. 更新能量方程*/
		UpdateEnergyEquation(p4est);

		/*10. 更新状态方程*/
		UpdateEquationOfState(p4est);

		/*11. 计算声速*/
		ComputeSoundSpeed(p4est);
	}
	p4est_data->used_dt = p4est_data->delta_time;
}












static void quadrant_copy_coordx_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	sc_array_t		*array_data = (sc_array_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset;
	CCorner_data	*cndata = (CCorner_data *)&quad_data->m_cndata;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;
	arrayoffset = P4EST_CHILDREN * local_id;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double  *this_ptr = (double *)sc_array_index(array_data, arrayoffset + index0);
		this_ptr[0] = m_vara->VecCnData[idcnCoords_lag][i].x;
	}
}

static void quadrant_copy_cell_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	vtu_cell_data_t	*m_cell_data = (vtu_cell_data_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset, corner_arrayoffset;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;

	arrayoffset = local_id;
	corner_arrayoffset = CNDIM*local_id;
	double		*p_val = (double *)sc_array_index(m_cell_data->pressure_array, arrayoffset);/*压力*/
	double		*t_val = (double *)sc_array_index(m_cell_data->temperature_array, arrayoffset);/*温度*/
	double		*rho_val = (double *)sc_array_index(m_cell_data->density_array, arrayoffset);/*密度*/
	double		*ie_val = (double *)sc_array_index(m_cell_data->internal_energy_array, arrayoffset);/*内能*/

	*p_val = m_vara->DouCData[idPressure_lag];
	*t_val = 0.0;
	*rho_val = m_vara->DouCData[idDensity_lag];
	*ie_val = m_vara->DouCData[idInternalEnergy_lag];
}

static void quadrant_copy_variable_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	vtu_cell_data_t	*m_cell_data = (vtu_cell_data_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset, corner_arrayoffset;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;

	arrayoffset = local_id;
	corner_arrayoffset = CNDIM * local_id;
	double		*p_val = (double *)sc_array_index(m_cell_data->pressure_array, arrayoffset);/*压力*/
	double		*t_val = (double *)sc_array_index(m_cell_data->temperature_array, arrayoffset);/*温度*/
	double		*rho_val = (double *)sc_array_index(m_cell_data->density_array, arrayoffset);/*密度*/
	double		*ie_val = (double *)sc_array_index(m_cell_data->internal_energy_array, arrayoffset);/*内能*/

	*p_val = m_vara->DouCData[idPressure_lag];
	*t_val = 0.0;
	*rho_val = m_vara->DouCData[idDensity_lag];
	*ie_val = m_vara->DouCData[idInternalEnergy_lag];
	for (int i = 0; i < CNDIM; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double *coordx_val = (double *)sc_array_index(m_cell_data->coordx, corner_arrayoffset + index0);/*x坐标*/
		coordx_val[0] = m_vara->VecCnData[idcnCoords_lag][i].x;

		double *coordy_val = (double *)sc_array_index(m_cell_data->coordy, corner_arrayoffset + index0);/*y坐标*/
		coordy_val[0] = m_vara->VecCnData[idcnCoords_lag][i].y;

		double *velox_val = (double *)sc_array_index(m_cell_data->velox, corner_arrayoffset + index0);/*x速度*/
		velox_val[0] = m_vara->VecCnData[idcnVelocity_lag][i].x;

		double *veloy_val = (double *)sc_array_index(m_cell_data->veloy, corner_arrayoffset + index0);/*y速度*/
		veloy_val[0] = m_vara->VecCnData[idcnVelocity_lag][i].y;
	}
}

/*children*/
/*----------------------------------*/
/*|                 |               |*/
/*|    child3       |      child4   |*/
/*------------------|---------------|*/
/*|                 |               |*/
/*|    child1       |      child2   |*/
/*------------------|---------------|*/
static void
GetRefineCornerCoords(const CDoubleVector &coordLB,
	const CDoubleVector &coordLU,
	const CDoubleVector &coordRU,
	const CDoubleVector &coordRB,
	CDoubleVector children_coord[P4EST_CHILDREN*CNDIM])
{
	enum edgeEnum { LEFT, UP, RIGHT, BOTTOM };
	CDoubleVector	coord[CNDIM];
	coord[quad_data_t::EnumCorner::LEFTBOTTOM] = coordLB;
	coord[quad_data_t::EnumCorner::LEFTUP] = coordLU;
	coord[quad_data_t::EnumCorner::RIGHTUP] = coordRU;
	coord[quad_data_t::EnumCorner::RIGHTBOTTOM] = coordRB;

	CDoubleVector	EdgeMiddle[CNDIM], AverCentroid;
	for (int k = 0; k < CNDIM; k++)
	{
		int knext = GeometryAlg::GetCircleNext(CNDIM, k);
		EdgeMiddle[k] = 0.5*(coord[k] + coord[knext]);
	}
	AverCentroid = GeometryAlg::GetPolyCenterByAverage(coord);

	/*child 1*/
	children_coord[quad_data_t::EnumCorner::LEFTBOTTOM] = coord[quad_data_t::EnumCorner::LEFTBOTTOM];
	children_coord[quad_data_t::EnumCorner::LEFTUP] = EdgeMiddle[edgeEnum::LEFT];
	children_coord[quad_data_t::EnumCorner::RIGHTUP] = AverCentroid;
	children_coord[quad_data_t::EnumCorner::RIGHTBOTTOM] = EdgeMiddle[edgeEnum::BOTTOM];

	/*child 2*/
	children_coord[CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = EdgeMiddle[edgeEnum::BOTTOM];
	children_coord[CNDIM + quad_data_t::EnumCorner::LEFTUP] = AverCentroid;
	children_coord[CNDIM + quad_data_t::EnumCorner::RIGHTUP] = EdgeMiddle[edgeEnum::RIGHT];
	children_coord[CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = coord[quad_data_t::EnumCorner::RIGHTBOTTOM];

	/*child 3*/
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = EdgeMiddle[edgeEnum::LEFT];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::LEFTUP] = coord[quad_data_t::EnumCorner::LEFTUP];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::RIGHTUP] = EdgeMiddle[edgeEnum::UP];
	children_coord[2 * CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = AverCentroid;

	/*child 4*/
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::LEFTBOTTOM] = AverCentroid;
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::LEFTUP] = EdgeMiddle[edgeEnum::UP];
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::RIGHTUP] = coord[quad_data_t::EnumCorner::RIGHTUP];
	children_coord[3 * CNDIM + quad_data_t::EnumCorner::RIGHTBOTTOM] = EdgeMiddle[edgeEnum::RIGHT];
	return;
}

static void
GetRefineCornerVelos(const CDoubleVector velo[CNDIM], CDoubleVector children_velo[P4EST_CHILDREN*CNDIM])
{
	
	return;
}

/* Initialize the state variables of incoming quadrants from outgoing quadrants (网格细化和聚合时数据操作)

* The functions p4est_refine_ext(), p4est_coarsen_ext(), and
* p4est_balance_ext() take as an argument a p4est_replace_t callback function,
* which allows one to setup the quadrant data of incoming quadrants from the
* data of outgoing quadrants, before the outgoing data is destroyed. This
* function matches the p4est_replace_t prototype.
*
* In this cell-centered Lagrangian scheme, we first assume the constant distribution
* of variables during refining, and during coarsening*/
static void
Lagrangian_replace_quads(p4est_t * p4est, p4est_topidx_t which_tree,
	int num_outgoing,
	p4est_quadrant_t *outgoing[],
	int num_incoming,
	p4est_quadrant_t *incoming[])
{
	enum edgeEnum { LEFT, UP, RIGHT, BOTTOM };
	enum m_geometry_id {m_coord, m_velo};
	enum m_physical_id {m_density, m_internal_energy};
	enum m_which_child {child1, child2, child3, child4};
	quad_data_t			*parent_data, *child_data, *child_data1, *child_data2, *child_data3, *child_data4;
	CVariable			*child_vara;
	p4est_data_t		*p4est_data = (p4est_data_t *)p4est->user_pointer;

	if (num_outgoing > 1)
	{
		/* this is coarsening */

		parent_data = (quad_data_t *)incoming[0]->p.user_data;
		child_data1 = (quad_data_t *)outgoing[0]->p.user_data;
		child_data2 = (quad_data_t *)outgoing[1]->p.user_data;
		child_data3 = (quad_data_t *)outgoing[2]->p.user_data;
		child_data4 = (quad_data_t *)outgoing[3]->p.user_data;

		/*标记该父网格刚刚经历了粗化*/
		parent_data->m_vara.IntCData[idCoarseningTag] = p4est_data_t::CoarseningEnum::CoarsenedJustNow;

		/*重新确定父网格隅角坐标*/
		/*children*/
		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		for (int idIndex = idcnCoords_cur; idIndex <= idcnVelocity_lag; idIndex++)
		{
			parent_data->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::LEFTBOTTOM] =
				child_data1->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::LEFTBOTTOM];
			parent_data->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::LEFTUP] =
				child_data3->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::LEFTUP];
			parent_data->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::RIGHTUP] =
				child_data4->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::RIGHTUP];
			parent_data->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::RIGHTBOTTOM] =
				child_data2->m_vara.VecCnData[idIndex][quad_data_t::EnumCorner::RIGHTBOTTOM];
		}

		int idChildIndex;

		/*粗化后，将children的几何数据存储在parent的ChildrenCnGeomVara数组中*/
		for (int idIndex = m_geometry_id::m_coord; idIndex <= m_geometry_id::m_velo; idIndex++)
		{
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
					child_data1->m_vara.VecCnData[idChildIndex][cnid];
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child2][cnid] =
					child_data2->m_vara.VecCnData[idChildIndex][cnid];
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child3][cnid] =
					child_data3->m_vara.VecCnData[idChildIndex][cnid];
			}
			for (int cnid = 0; cnid < CNDIM; cnid++)
			{
				parent_data->m_vara.ChildrenCnGeomVara[idIndex][m_which_child::child4][cnid] =
					child_data4->m_vara.VecCnData[idChildIndex][cnid];
			}
		}

		/*粗化后，将children的物理量数据（密度和内能）存储在parent的ChildrenPhysicalVara数组中*/
		for (int idIndex = m_physical_id::m_density; idIndex <= m_physical_id::m_internal_energy; idIndex++)
		{
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
				child_data1->m_vara.DouCData[idChildIndex];
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child2] =
				child_data2->m_vara.DouCData[idChildIndex];
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child3] =
				child_data3->m_vara.DouCData[idChildIndex];
			parent_data->m_vara.ChildrenPhysicalVara[idIndex][m_which_child::child4] =
				child_data4->m_vara.DouCData[idChildIndex];
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

		parent_data->m_vara.DouCData[idMass] = child_data1->m_vara.DouCData[idMass] +
			child_data2->m_vara.DouCData[idMass] +
			child_data3->m_vara.DouCData[idMass] +
			child_data4->m_vara.DouCData[idMass]; /*质量*/

		/*体积和密度*/
		CDoubleVector m_cell_coord[CNDIM];
		for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = parent_data->m_vara.VecCnData[idcnCoords_cur][i]; }
		parent_data->m_vara.DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
		parent_data->m_vara.DouCData[idDensity_cur] = parent_data->m_vara.DouCData[idMass] / parent_data->m_vara.DouCData[idVolume];
		parent_data->m_vara.DouCData[idDensity_lag] = parent_data->m_vara.DouCData[idDensity_cur];
		CDoubleVector center_point;
		center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
		parent_data->m_vara.VecCData[idCentroidCoord_cur] = center_point;

		/*根据总动量守恒规律，求解粗化后的父网格动量*/
		parent_data->m_vara.VecCData[idCentroidVelo_cur] = (child_data1->m_vara.DouCData[idMass] * child_data1->m_vara.VecCData[idCentroidVelo_cur] +
			child_data2->m_vara.DouCData[idMass] * child_data2->m_vara.VecCData[idCentroidVelo_cur] +
			child_data3->m_vara.DouCData[idMass] * child_data3->m_vara.VecCData[idCentroidVelo_cur] +
			child_data4->m_vara.DouCData[idMass] * child_data4->m_vara.VecCData[idCentroidVelo_cur])
			/ parent_data->m_vara.DouCData[idMass];
		parent_data->m_vara.VecCData[idCentroidVelo_lag] = parent_data->m_vara.VecCData[idCentroidVelo_cur];

		parent_data->m_vara.DouCData[idGamma] = (
			child_data1->m_vara.DouCData[idGamma] +
			child_data2->m_vara.DouCData[idGamma] +
			child_data3->m_vara.DouCData[idGamma] +
			child_data4->m_vara.DouCData[idGamma]) / P4EST_CHILDREN;

		/*根据总能不变原理，求解粗化后的父网格总能量*/
		parent_data->m_vara.DouCData[idTotalEnergy_cur] = (
			child_data1->m_vara.DouCData[idMass] * child_data1->m_vara.DouCData[idTotalEnergy_cur] +
			child_data2->m_vara.DouCData[idMass] * child_data2->m_vara.DouCData[idTotalEnergy_cur] +
			child_data3->m_vara.DouCData[idMass] * child_data3->m_vara.DouCData[idTotalEnergy_cur] +
			child_data4->m_vara.DouCData[idMass] * child_data4->m_vara.DouCData[idTotalEnergy_cur])
			/ parent_data->m_vara.DouCData[idMass];
		parent_data->m_vara.DouCData[idTotalEnergy_lag] = parent_data->m_vara.DouCData[idTotalEnergy_cur];

		/*根据总能和动量，求解内能*/
		parent_data->m_vara.DouCData[idInternalEnergy_cur] = parent_data->m_vara.DouCData[idTotalEnergy_cur] -
			0.5 * (pow(parent_data->m_vara.VecCData[idCentroidVelo_cur].x, 2) + pow(parent_data->m_vara.VecCData[idCentroidVelo_cur].y, 2));
		if (parent_data->m_vara.DouCData[idInternalEnergy_cur] > m_eps)
		{

		}
		else
		{
			P4EST_GLOBAL_PRODUCTIONF("The value of internal energy is illegal in refining!\n");
			abort();
		}
		parent_data->m_vara.DouCData[idInternalEnergy_lag] = parent_data->m_vara.DouCData[idTotalEnergy_lag] -
			0.5 * (pow(parent_data->m_vara.VecCData[idCentroidVelo_lag].x, 2) + pow(parent_data->m_vara.VecCData[idCentroidVelo_lag].y, 2));
		parent_data->m_vara.DouCData[idInternalEnergy_lag] = parent_data->m_vara.DouCData[idInternalEnergy_cur];

		/*更新压力、声速*/
		parent_data->m_vara.DouCData[idPressure_lag] = PhysicalAlg::EquationOfState(
			parent_data->m_vara.DouCData[idGamma],
			parent_data->m_vara.DouCData[idDensity_lag],
			parent_data->m_vara.DouCData[idInternalEnergy_lag]);
		parent_data->m_vara.DouCData[idPressure_cur] = parent_data->m_vara.DouCData[idPressure_lag];
		parent_data->m_vara.DouCData[idSoundSpeed] = PhysicalAlg::CalculateSoundSpeed(
			parent_data->m_vara.DouCData[idGamma],
			parent_data->m_vara.DouCData[idPressure_cur],
			parent_data->m_vara.DouCData[idDensity_cur]);
	}
	else
	{
		/* this is refining */

		/*children*/
		/*----------------------------------*/
		/*|                 |               |*/
		/*|    child3       |      child4   |*/
		/*------------------|---------------|*/
		/*|                 |               |*/
		/*|    child1       |      child2   |*/
		/*------------------|---------------|*/
		CDoubleVector children_coord[P4EST_CHILDREN][CNDIM];

		parent_data = (quad_data_t *)outgoing[0]->p.user_data;

		double children_total_energy = 0.;
		double children_energy_per_mass = 0.;
		double children_total_mass = 0.;
		for (int i = 0; i < P4EST_CHILDREN; i++)
		{
			child_data = (quad_data_t *)incoming[i]->p.user_data;

			p4est_qcoord_t qx = incoming[i]->x;
			p4est_qcoord_t qy = incoming[i]->y;

			/*1. 一阶精度，继承父网格的所有物理量*/
			for (int j = 0; j < idDoubleCellVariableNum; j++)
			{
				/***************double 型网格中心量*****************/
				child_data->m_vara.DouCData[j] = parent_data->m_vara.DouCData[j];
				if (parent_data->m_vara.DouCData[idInternalEnergy_cur] > m_eps)
				{
				}
				else
				{
					P4EST_GLOBAL_PRODUCTIONF("The cihldren internal energy is illegal in refining!\n");
					abort();
				}
			}
			for (int j = idReconstructPressure; j < idDoubleCornerVariableNum; j++)
			{
				for (int k = 0; k < CNDIM; k++)
				{
					/***************double 型网格隅角量*****************/
					child_data->m_vara.DouCnData[j][k] = parent_data->m_vara.DouCnData[j][k];
				}
			}
			for (int j = 0; j < idVectorCellVariableNum; j++)
			{
				/***************CDoubleVector 型网格中心量*****************/
				child_data->m_vara.VecCData[j] = parent_data->m_vara.VecCData[j];
			}
			for (int j = 0; j < idVectorCornerVariableNum; j++)
			{
				for (int k = 0; k < CNDIM; k++)
				{
					/***************CDoubleVector 型网格隅角量*****************/
					child_data->m_vara.VecCnData[j][k] = parent_data->m_vara.VecCnData[j][k];
				}
			}

			/*2. 获得子网格的坐标和速度*/
			int idParentGeometry;
			for (int idChildrenIndex = idcnCoords_cur; idChildrenIndex <= idcnVelocity_lag; idChildrenIndex++)
			{
				if (idChildrenIndex == idcnCoords_cur || idChildrenIndex == idcnCoords_lag || idChildrenIndex == idcnCoords_half)
				{
					idParentGeometry = m_geometry_id::m_coord;
				}
				if (idChildrenIndex == idcnVelocity_cur || idChildrenIndex == idcnVelocity_lag)
				{
					idParentGeometry = m_geometry_id::m_velo;
				}

				for (int cnid = 0; cnid < CNDIM; cnid++)
				{
					child_data->m_vara.VecCnData[idChildrenIndex][cnid] =
						parent_data->m_vara.ChildrenCnGeomVara[idParentGeometry][i][cnid];

					if (idChildrenIndex == idcnCoords_lag)
					{
						children_coord[i][cnid] = parent_data->m_vara.ChildrenCnGeomVara[idParentGeometry][i][cnid];
					}
				}
			}

			/*获得子网格的密度和内能*/
			int idParentPhysical;
			for (int idChildrenIndex = idDensity_cur; idChildrenIndex <= idInternalEnergy_lag; idChildrenIndex++)
			{
				if (idChildrenIndex == idDensity_cur || idChildrenIndex == idDensity_half || idChildrenIndex == idDensity_lag)
				{
					idParentPhysical = m_physical_id::m_density;
				}
				if (idChildrenIndex == idInternalEnergy_cur || idChildrenIndex == idInternalEnergy_half || idChildrenIndex == idInternalEnergy_lag)
				{
					idParentPhysical = m_physical_id::m_internal_energy;
				}

				child_data->m_vara.DouCData[idChildrenIndex] =
					parent_data->m_vara.ChildrenPhysicalVara[idParentPhysical][i];
				double m_value = parent_data->m_vara.ChildrenPhysicalVara[idParentPhysical][i];
				if (child_data->m_vara.DouCData[idChildrenIndex] > m_eps)
				{
				}
				else
				{
					P4EST_GLOBAL_PRODUCTIONF("The cihldren value of idChildrenIndex is illegal in refining!\n");
					abort();
				}
			}
			
			for (int idVCn = idcnCoords_cur; idVCn <= idcnCoords_lag; idVCn++)
			{
				int idVC;
				switch (idVCn)
				{
				case idcnCoords_cur:
					idVC = idCentroidCoord_cur;
					break;
				case idcnCoords_half:
					idVC = idCentroidCoord_half;
					break;
				case idcnCoords_lag:
					idVC = idCentroidCoord_lag;
					break;
				default:
					break;
				}
				CDoubleVector m_cell_coord[CNDIM];
				for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = child_data->m_vara.VecCnData[idVCn][i]; }
				CDoubleVector center_point;
				center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
				child_data->m_vara.VecCData[idVC] = center_point;

				if (idVCn == idcnCoords_cur)
				{
					child_data->m_vara.DouCData[idVolume] = GeometryAlg::CalculateCellVolume(p4est_data->coord_type, m_cell_coord);
					child_data->m_vara.DouCData[idMass] = PhysicalAlg::CalculateCellMass(
						child_data->m_vara.DouCData[idVolume], child_data->m_vara.DouCData[idDensity_cur]);
				}
			}
			children_total_energy += child_data->m_vara.DouCData[idMass] * child_data->m_vara.DouCData[idTotalEnergy_lag];
			children_total_mass += child_data->m_vara.DouCData[idMass];
			children_energy_per_mass += child_data->m_vara.DouCData[idTotalEnergy_lag];

			child_vara = (CVariable *)&child_data->m_vara;
			generate_children_info_from_parent(p4est_data, child_vara);
		}

		double parent_total_energy = parent_data->m_vara.DouCData[idMass] * parent_data->m_vara.DouCData[idTotalEnergy_lag];
		double parent_energy_per_mass = parent_data->m_vara.DouCData[idTotalEnergy_lag];
		double parent_total_mass = parent_data->m_vara.DouCData[idMass];
		if (abs((parent_total_energy - children_total_energy)/ parent_total_energy) > 1e-10)
		{
			P4EST_GLOBAL_PRODUCTIONF("The total energy is not conservative during refining!\n");
			if (abs((parent_total_mass - children_total_mass) / parent_total_mass) > 1e-10)
			{
				P4EST_GLOBAL_PRODUCTIONF("In the mean time, the total mass is not conservative during refining!\n");
				P4EST_GLOBAL_PRODUCTIONF("error is %.10lf\n", (parent_total_mass - children_total_mass) / parent_total_mass);
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("However, the total mass is conservative during refining!\n");
			}

			if (abs((parent_energy_per_mass - children_energy_per_mass) / parent_energy_per_mass) > 1e-10)
			{
				P4EST_GLOBAL_PRODUCTIONF("In the mean time, the energy per mass is not conservative during refining!\n");
				P4EST_GLOBAL_PRODUCTIONF("error is %.10lf\n", (parent_energy_per_mass - children_energy_per_mass) / parent_energy_per_mass);
			}
			else
			{
				P4EST_GLOBAL_PRODUCTIONF("However, the energy per mass is conservative during refining!\n");
			}

		}
	}
	return;
}

static void quadrant_copy_coordy_to_array_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	sc_array_t		*array_data = (sc_array_t *)user_data;
	p4est_t			*p4est = info->p4est;
	p4est_tree_t	*tree;
	quad_data_t		*quad_data = (quad_data_t *)info->quad->p.user_data;
	p4est_topidx_t	which_tree = info->treeid;
	p4est_locidx_t	local_id = info->quadid;
	p4est_locidx_t	arrayoffset;
	CCorner_data	*cndata = (CCorner_data *)&quad_data->m_cndata;
	CVariable		*m_vara = (CVariable *)&quad_data->m_vara;

	tree = p4est_tree_array_index(p4est->trees, which_tree);
	local_id += tree->quadrants_offset;
	arrayoffset = P4EST_CHILDREN * local_id;
	for (int i = 0; i < P4EST_CHILDREN; i++) {
		int index0 = convert_user_define_index_to_which_corner(i);
		double  *this_ptr = (double *)sc_array_index(array_data, arrayoffset + index0);
		this_ptr[0] = m_vara->VecCnData[idcnCoords_lag][i].y;
	}
}

/*粗化后，给网格打标签，表示这个网格刚刚经历了粗化*/
static void
quadrant_set_default_coarsening_tag_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	
	/*默认未粗化*/
	data->m_vara.IntCData[idCoarseningTag] = p4est_data_t::CoarseningEnum::NotCoarsenedJustNow;

	/*默认允许粗化*/
	data->m_vara.IntCData[idAllowCoarsening] = p4est_data_t::CoarseningEnum::CoarsingAllowed;
}

static void set_default_coarsening_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_set_default_coarsening_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

/*除了凹四边形，其他都可以细化*/
static void
quadrant_set_default_refining_tag_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;

	CDoubleVector m_coord[CNDIM];
	
	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		m_coord[cnid] = m_vara->VecCnData[idcnCoords_lag][CNDIM - 1 - cnid];

	}
	int IsConcaveQuad = GeometryAlg::is_concave_quad(m_coord);
	m_vara->IntCData[idAllowRefining] = IsConcaveQuad;

	if (IsConcaveQuad < 0)
	{
		/*凸四边形*/
		data->m_vara.IntCData[idAllowRefining] = p4est_data_t::RefiningEnum::RefiningAllowed;
	}
	else
	{
		/*凹四边形*/

	}
}

/*预测哪些四边形将要被细化*/
static void
quadrant_predict_refining_quads_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	int idCPara;
	p4est_qcoord_t		qx = info->quad->x;
	p4est_qcoord_t		qy = info->quad->y;
	int					level = info->quad->level;
	p4est_qcoord_t		length = P4EST_QUADRANT_LEN(level);

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	if (level < p4est_data->minus_level)
	{
		m_vara->IntCData[idAllowRefining] = p4est_data_t::RefiningEnum::MustRefing;
	}
	if (level >= p4est_data->max_level)
	{
		m_vara->IntCData[idAllowRefining] = p4est_data_t::RefiningEnum::RefiningNotAllowed;
	}

	if (m_vara->DouCData[idCPara] > p4est_data->refine_err)
	{
		m_vara->IntCData[idAllowRefining] = p4est_data_t::RefiningEnum::MustRefing;
	}
	else
	{
		m_vara->IntCData[idAllowRefining] = p4est_data_t::RefiningEnum::RefiningNotAllowed;
	}
}

static void 
set_default_refining_tag(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_set_default_refining_tag_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void 
set_allowing_coarsening_tag(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		NULL,
		quadrant_whether_allowing_coarsening_from_edge_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		NULL,
		NULL,
		NULL,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		quadrant_whether_allowing_coarsening_from_corner_callback);
}

static void 
Predict_refining_Quads(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	/*预测那些quad将要被细化*/
	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_predict_refining_quads_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void
quadrant_reset_hanging_info_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;

	for (int cnid = 0; cnid < CNDIM; cnid++)
	{
		data->points[cnid].IsHanging = false;
		data->points[cnid].AddDiss = false;
		data->points[cnid].add_dissipation_parent = false;
	}

	for (int enid = 0; enid < CNDIM; enid++)
	{
		data->m_pc_edge_data[enid].addDiss = false;
	}
}

//the gradient is reset to zero at the beginning of each timestep
static void
quadrant_set_gradient_zero_estimate_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t*)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;
	p4est_t			*p4est = info->p4est;

	int idCPara, idEPara, idCNPara;

	switch (p4est_data->refine_coarsen_enum)
	{
	case RefineCriteria::PressureGradient:
		idCPara = idCPressureGradient;
		idEPara = idEPressureGradient;
		idCNPara = idCNPressGradient;
		break;
	case RefineCriteria::DensityGradient:
		idCPara = idCDensityGradient;
		idEPara = idERhoGradient;
		idCNPara = idCNRhoGradient;
		break;
	case RefineCriteria::Distance:
		return;
	default:
		break;
	}

	/*the gradient value is set to zero*/
	m_vara->DouCData[idCPara] = 0.;

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->DouEData[idEPara][i] = 0.;
	}

	for (int i = 0; i < CNDIM; i++)
	{
		m_vara->DouCnData[idCNPara][i] = 0.;
	}
}

static void
refresh_after_balance(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		NULL,
		quadrant_update_after_balance_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		NULL,
		NULL,
		quadrant_reset_hanging_info_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

/*粗化后作后处理*/
static void
postprocess_after_coarsening(p4est_t *p4est)
{
	p4est_iterate(p4est,
		NULL,
		NULL,
		NULL,
		quadrant_update_after_coarsening_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void 
Gradient_estimate(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	p4est_iterate(p4est,
		ghost,
		(void *)ghost_data,
		quadrant_set_gradient_zero_estimate_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	/*estimate the gradient across each edge*/
	p4est_iterate(p4est,
		ghost,
		(void *)ghost_data,
		NULL,
		quadrant_edge_minmod_estimate_callback,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);

	p4est_iterate(p4est,
		ghost,
		(void *)ghost_data,
		NULL,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		quadrant_corner_minmod_estimate_callback);

	/*the press gradient of each cell is the max value of that across four edges*/
	p4est_iterate(p4est,
		NULL,
		(void *)p4est_data,
		quadrant_cell_minmod_estimate_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,

#endif
		NULL);
}

static void PreProcess(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data)
{
	/*计算梯度，用于自适应加密、减疏判据*/
	Gradient_estimate(p4est, ghost, ghost_data);
	/*设定粗化标签，用于标记网格是否经历了细化1-粗化-细化2(balance)操作中的粗化阶段*/
	set_default_coarsening_tag(p4est);

	/*根据四边形是否未凹四边形，判断允许细化*/
	set_default_refining_tag(p4est);
}





























static void write_balance_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_balance_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est,NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void write_coarsen_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_coarsen_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est, NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void write_refine_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	sc_array_t			*coord_x;
	sc_array_t			*coord_y;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	snprintf(filename, BUFSIZ, P4EST_STRING "_refine_%04d", time_step);

	numquads = p4est->local_num_quadrants;

	coord_x = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);
	coord_y = sc_array_new_size(sizeof(double), numquads*P4EST_CHILDREN);

	p4est_iterate(p4est, NULL,
		(void *)coord_x,
		quadrant_copy_coordx_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	p4est_iterate(p4est, NULL,
		(void *)coord_y,
		quadrant_copy_coordy_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	context = p4est_vtk_write_point_dataf(context, 2, 0, "coordinate_X", coord_x,
		"coordinate_Y", coord_y, context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x);
	sc_array_destroy(coord_y);

}

static void quadrant_write_distance_profiles_callback(p4est_iter_volume_info_t *info, void *user_data)
{
	p4est_data_t		*p4est_data = (p4est_data_t *)info->p4est->user_pointer;
	quad_data_t		*data = (quad_data_t *)info->quad->p.user_data;
	CVariable		*m_vara = (CVariable *)&data->m_vara;

	CDoubleVector m_cell_coord[CNDIM];
	for (int i = 0; i < CNDIM; i++) { m_cell_coord[i] = m_vara->VecCnData[idcnCoords_lag][i]; }

	CDoubleVector center_point;
	center_point = GeometryAlg::GetPolyCenter(m_cell_coord);
	double distance;

	if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::radiusType)
	{
		distance = sqrt(pow(center_point.x, 2) + pow(center_point.y,2));
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::xType)
	{
		distance = fabs(center_point.x);
	}
	else if (p4est_data->profiletype == p4est_data_t::DistanceProfileType::yType)
	{
		distance = fabs(center_point.y);
	}
	p4est_data->DistanceFile << blank << blank << distance <<
		blank << blank << m_vara->DouCData[idDensity_lag] <<
		blank << blank << m_vara->DouCData[idPressure_lag] <<
		blank << blank << m_vara->DouCData[idInternalEnergy_lag] <<
		blank << blank << m_vara->DouCData[idTotalEnergy_lag] << endl;
}

static void write_distance_profiles(p4est_t *p4est)
{
	p4est_data_t		*p4est_data = (p4est_data_t*)p4est->user_pointer;
	int ret;
#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);//Linux等系统
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}
	p4est_iterate(p4est, NULL,
		(void *)p4est_data,
		quadrant_write_distance_profiles_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);
}

static void write_solution(p4est_t *p4est, const int &time_step)
{
	char				filename[BUFSIZ] = "";
	int					retval;
	p4est_locidx_t		numquads;
	p4est_vtk_context_t	*context;
	int					ret;

#ifdef _WIN32
	ret = _mkdir("output");
	if (ret != 0 && errno != EEXIST) {
#else
	ret = mkdir("output", 0777);//Linux等系统
	if (ret != 0 && errno != EEXIST) {
#endif
		perror("Error creating directory");
	}

#ifdef _WIN32
	const char* path_format = "output\\" P4EST_STRING "_Lagrangian_%04d";
#else
	const char* path_format = "output/"P4EST_STRING "_Lagrangian_%04d";
#endif

	snprintf(filename, BUFSIZ, path_format, time_step);

	numquads = p4est->local_num_quadrants;

	sc_array_t		*coord_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*coord_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_x_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*velo_y_array = sc_array_new_size(sizeof(double), numquads*CNDIM);
	sc_array_t		*pressure_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*temperature_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*rho_array = sc_array_new_size(sizeof(double), numquads);
	sc_array_t		*internal_energy_array = sc_array_new_size(sizeof(double), numquads);

	vtu_cell_data_t		m_cell_data;
	m_cell_data.density_array = rho_array;
	m_cell_data.pressure_array = pressure_array;
	m_cell_data.temperature_array = temperature_array;
	m_cell_data.internal_energy_array = internal_energy_array;
	m_cell_data.coordx = coord_x_array;
	m_cell_data.coordy = coord_y_array;
	m_cell_data.velox = velo_x_array;
	m_cell_data.veloy = velo_y_array;



















	p4est_iterate(p4est, NULL,
		&m_cell_data,
		quadrant_copy_variable_to_array_callback,
		NULL,
#ifdef  P4_TO_P8
		NULL,
#endif
		NULL);

	context = p4est_vtk_context_new(p4est, filename);
	p4est_vtk_context_set_scale(context, 0.99);
	SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk:Error:writing vtk header");
	context = p4est_vtk_write_header(context);
	
	context = p4est_vtk_write_cell_dataf(
		context, /*vtk上下文*/
		0,/*是否输出树ID（0/1）*/
		1,/*是否输出象限层级(0/1)*/
		1,/*是否输出MPI秩(0/1)*/
		0,/*对MPI秩取模(0表示不包装)*/
		3,/*标量数据集数量(如压力)*/
		0,/*向量数据集数量*/
		"Pressure", 
		pressure_array,
		"density", 
		rho_array,
		"internal_energy",
		internal_energy_array,
		context
	);
	SC_CHECK_ABORT(context != NULL,
		P4EST_STRING "_vtk:Error:writing cell data");

	context = p4est_vtk_write_point_dataf(context, 4, 0,
		"NodeX", coord_x_array,
		"NodeY", coord_y_array,
		"NodeU", velo_x_array,
		"NodeV", velo_y_array,
		context);
	retval = p4est_vtk_write_footer(context);
	SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk:Error:writing footer");
	sc_array_destroy(coord_x_array);
	sc_array_destroy(coord_y_array);
	sc_array_destroy(velo_x_array);
	sc_array_destroy(velo_y_array);
	sc_array_destroy(pressure_array);
	sc_array_destroy(rho_array);
	sc_array_destroy(internal_energy_array);
	sc_array_destroy(temperature_array);
}

/** Timestep the Lagrangian Algs.
*
* Update the state, refine, repartition, and write the solution to file.
*
* \param [in,out] p4est the forest, whose state is updated
* \param [in] start_time    simulation start time
* \param [in] end_time      simulation end time
*/
static void advance_time_step(p4est_t * p4est, double start_time, double end_time)
{
	double              t = start_time;
	double              dt = 0.;
	p4est_ghost_t		*ghost;
	p4est_data_t		*ghost_data;
	p4est_data_t		*p4est_data = (p4est_data_t *)p4est->user_pointer;
	int					recursive = 0;
	int					allowed_level = p4est_data->max_level;
	int					callbackorphans = 0;
	int					allowcoarsening = 1;/*只能粗化一层*/

	/*create the ghost quadrants*/
	ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FULL);
	/*create space for storing the ghost data*/
	ghost_data = P4EST_ALLOC(p4est_data_t, ghost->ghosts.elem_count);
	/*synchronize（同步） the ghost data*/
	p4est_ghost_exchange_data(p4est, ghost, ghost_data);

	for (t = start_time; t < end_time; t += p4est_data->delta_time)
	{
		p4est_data->current_step += 1;
		int current_output_index = (int)(p4est_data->current_time / p4est_data->write_interval_time);

		/*预处理*/
		PreProcess(p4est, ghost, ghost_data);

		/*refine*/
		if (p4est_data->current_step && !(p4est_data->current_step%p4est_data->refine_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)

		{
			p4est_refine_ext(p4est, recursive, allowed_level,
				Lagrangian_refine_err_estimate, NULL,
				Lagrangian_replace_quads);

			set_allowing_coarsening_tag(p4est, ghost, ghost_data);

			p4est_coarsen_ext(p4est, recursive, callbackorphans,
			Lagrangian_coarsen_err_estimate, NULL,
			Lagrangian_replace_quads);

			StatTotalEnergyError(p4est);
			p4est_balance_ext(p4est, P4EST_CONNECT_CORNER, NULL,
				Lagrangian_replace_quads);

			p4est_ghost_destroy(ghost);
			P4EST_FREE(ghost_data);
			ghost = NULL;
			ghost_data = NULL;
		}

		/*repartition*/
		if (p4est_data->current_step &&
			!(p4est_data->current_step%p4est_data->repartition_period)
			&& p4est_data->current_time>p4est_data->refine_coarsen_time)
		{
			p4est_partition(p4est, allowcoarsening, NULL);
			if (ghost) {
				p4est_ghost_destroy(ghost);
				P4EST_FREE(ghost_data);
				ghost = NULL;
				ghost_data = NULL;
			}
		}

		/*synchronize the ghost data*/
		if (!ghost)
		{
			ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FULL);
			ghost_data = P4EST_ALLOC(p4est_data_t, ghost->ghosts.elem_count);
			p4est_ghost_exchange_data(p4est, ghost, ghost_data);
		}

		/*refresh haning information in user defined data*/
		refresh_after_balance(p4est);
		/*更新影像区，防止refresh失效*/
		p4est_ghost_exchange_data(p4est, ghost, ghost_data);

		/* 预估时间步长*/
		if (p4est_data->equal_dt == false) { predict_timestep(p4est); }

		/*write out solution*/
		if (!(p4est_data->current_step % p4est_data->write_interval_step))
		{
			write_solution(p4est, p4est_data->current_step);
		}
		else if (current_output_index > p4est_data->last_output_index)
		{
			p4est_data->last_output_index = current_output_index;
			write_solution(p4est, p4est_data->current_step);
		}
		else if (p4est_data->current_time+ p4est_data->delta_time >= p4est_data->end_time)
		{
			write_solution(p4est, p4est_data->current_step);
		}

		/* 数值计算时间推进*/
		two_stage_Runge_Kutta(p4est, ghost, (void *)ghost_data);

		/*统计总能误差，用于验证格式是否满足总能量守恒*/
		StatTotalEnergyError(p4est);

		/* 接收数值解*/
		AcceptNumericalSolution(p4est);

		p4est_data->current_time = p4est_data->current_time + p4est_data->delta_time;

		/*打印到屏幕*/
		P4EST_GLOBAL_PRODUCTIONF("simulation_step= %d, delta_time = %.10lf, simulation_time = %.6lf \n",
			p4est_data->current_step, p4est_data->delta_time, p4est_data->current_time);
	}
	write_distance_profiles(p4est);
	P4EST_FREE(ghost_data);
	p4est_ghost_destroy(ghost);
}



















































int main(int argc, char **argv)
{
	int                 mpiret;
	sc_MPI_Comm         mpicomm;
	p4est_data_t		ctx;

	mpiret = sc_MPI_Init(&argc, &argv);
	SC_CHECK_MPI(mpiret);
	mpicomm = sc_MPI_COMM_WORLD;

	sc_init(mpicomm, 1, 1, NULL, SC_LP_ESSENTIAL);
	p4est_init(NULL, SC_LP_PRODUCTION);

	P4EST_GLOBAL_PRODUCTIONF("This is the p4est %dD demo for Lagrangian hydrodynamics\n", P4EST_DIM);

	// 1. 创建基础连接性（2D矩形域）
	p4est_connectivity_t *conn = p4est_connectivity_new_unitsquare();
	//p4est_connectivity_t *conn = p4est_connectivity_new_brick(70,30,0,0);

	// 2. 创建p4est实例，并通过回调函数init_condition给定初始条件，如Sedov、Noh问题的初始状态
	p4est_t *p4est = p4est_new_ext(mpicomm,				 // MPI通信域
		conn,					 // 连接性
		1,						 // 初始细化层级
		7,						 // 最小细化层级
		1,						 //均匀填充
		sizeof(quad_data_t), // 用户数据大小
		Lagrangian_init_condition,// 初始化回调（可空）
		(void *)(&ctx));          // 用户指针

	p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;

	/*recursive = 1, refine_fn is called for every existing and newly creatly quadrant.
	Otherwise, it is called for every existing quadrant.*/
	int recursive = 1;
	/*p4est_refine(p4est, recursive, Lagrangian_refine_err_estimate,
	Lagrangian_init_condition);
	p4est_coarsen(p4est, recursive, Lagrangian_coarsen_err_estimate,
	Lagrangian_init_condition);*/

	/*Partition: The quadrants are redistributed for equal element count. The
	* partition can optionally be modified such that a family of octants, which
	* are possibly ready for coarsening, are never split between processors.*/
	
	
	
	
	
	
	
	
	
	int partforcoarsen = 1;

	/* If we call the 2:1 balance we ensure that neighbours do not differ in size
	* by more than a factor of 2. This can optionally include digonal neighbors
	* across edges or corners as well; see p4est.h.*/
	p4est_balance(p4est, P4EST_CONNECT_CORNER, Lagrangian_init_condition);
	p4est_partition(p4est, partforcoarsen, NULL);

	//	/*测试隅角编号用*/
	//	p4est_iterate(p4est,
	//		NULL,          // 无需ghost层数据
	//		(void*)p4est_data,   // 将时间步作为用户参数传递
	//		NULL, // 更新回调函数
	//		NULL,
	//#ifdef P4_TO_P8
	//		NULL,                  /* there is no callback for the
	//							   edges between quadrants */
	//#endif
	//		quadrant_test_corner_callback);         // 测试隅角编号

	//step3_get_timestep(p4est);/*测试用*/
	//p4est_ghost_t *ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FULL);//对森林p4est采用角点方式构造影像区ghost

	// 5. 执行初始负载均衡
	//p4est_partition(p4est, 1, NULL);
	//p4est_balance(p4est, P4EST_CONNECT_FULL, NULL);

	//p4est_data_t *p4est_data = (p4est_data_t *)p4est->user_pointer;
	//p4est_data->start_time = 0.;
	//p4est_data->end_time = 1.;

	/*6. 时间步进*/
	advance_time_step(p4est,                    //森林
		p4est_data->start_time,    //起始时间
		p4est_data->end_time);     //终止时间

								   //// 6. 示例：执行坐标更新与同步
								   //	for (int step = 0; step < 5; ++step) {
								   //		if (p4est->mpirank == 0) {
								   //			printf("Step %d: Updating coordinates...\n", step);
								   //		}
								   //		update_coordinates(p4est);
								   //	}

	// 7. 资源清理
	p4est_destroy(p4est);
	p4est_connectivity_destroy(conn);

	sc_finalize();
	mpiret = sc_MPI_Finalize();
	SC_CHECK_MPI(mpiret);
	return 0;
}