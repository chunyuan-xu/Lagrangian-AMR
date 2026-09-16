#pragma once
#include "hydro/dissipative_weight_math.h"
#include "hydro/dissipative_vector_math.h"
#include "defines.h"
#include "variable.h"
#include "alg.h"
#include <cstdlib>
#include <cstdio>
// Isolated serial, planar control-volume experiment. This constrains only the
// SEMIDISCRETE nodal dissipative contribution, using current center velocity.
// It does not prove fully discrete cell positivity or entropy stability.
namespace DissipativeWeightsExperiment {
inline int mode(){static int m=std::getenv("AMR_DISSIPATIVE_WEIGHTS")?std::atoi(std::getenv("AMR_DISSIPATIVE_WEIGHTS")):0;return m;}
struct Stats {long long nodes=0,infeasible=0,candidates=0,applied=0,negative_before=0,negative_after=0,vector_infeasible=0,vector_candidates=0,vector_applied=0;double deficit_before=0,deficit_after=0,max_sum_error=0,max_force_residual=0,max_work_residual=0;};
inline Stats&stats(){static Stats s;return s;}
inline int&count_iteration(){static int i=fixed_iter_num-1;return i;}
inline void begin(p4est_t*f){if(!mode())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;SC_CHECK_ABORT(f->mpisize==1&&r.coord_type==p4est_data_t::MyCoordType::plane&&r.Scheme_type==p4est_data_t::MySchemeType::ControlVolume,"Dissipative-weight prototype requires serial planar CV");stats()=Stats{};}
inline std::array<CDoubleVector,3> forces(p4est_t*f,const CVariable*const v[3],const CDoubleMatrix M[3],const CDoubleVector&vh,const CDoubleVector&R,const DissipativeWeightMath::A&base,int iter){
 using namespace DissipativeWeightMath;A d,q;DissipativeVectorMath::VA duvector;
 for(int i=0;i<3;++i){const auto du=vh-v[i]->cell_vector(idCentroidVelo_cur);const auto md=GeometryAlg::MatrixDotVector(M[i],du);d[i]=du^md;q[i]=du^R;
  SC_CHECK_ABORT(std::isfinite(d[i])&&d[i]>=-1e-13*(std::abs(du.x*md.x)+std::abs(du.y*md.y)+1e-30),"Invalid nodal quadratic form");d[i]=std::max(0.,d[i]);duvector[i]={du.x,du.y};}
 const auto result=project(base,d,q);SC_CHECK_ABORT(result.valid,"Invalid capped-simplex projection");
 const A out=mode()==2&&result.feasible?result.weights:base;
 const auto vector_result=DissipativeVectorMath::project(base,duvector,d,{R.x,R.y});
 SC_CHECK_ABORT(vector_result.valid,"Invalid vector projection inputs");
 std::array<CDoubleVector,3> forces;
 for(int i=0;i<3;++i){forces[i]=out[i]*R;if(mode()==3&&vector_result.feasible)forces[i]=CDoubleVector(vector_result.force[i].x,vector_result.force[i].y);}
 if(iter==count_iteration()){auto&s=stats();++s.nodes;if(!result.feasible)++s.infeasible;if(result.changed)++s.candidates;if(mode()==2&&result.changed)++s.applied;
  if(!vector_result.feasible)++s.vector_infeasible;if(vector_result.changed)++s.vector_candidates;if(mode()==3&&vector_result.changed)++s.vector_applied;
  double sum=0,work=0,normscale=std::hypot(R.x,R.y);CDoubleVector force_sum(0.,0.);
  for(int i=0;i<3;++i){sum+=out[i];force_sum+=forces[i];work+=vh^forces[i];normscale+=std::hypot(forces[i].x,forces[i].y);const double a=d[i]-base[i]*q[i],b=d[i]-(duvector[i].x*forces[i].x+duvector[i].y*forces[i].y),tol=1e-12*(d[i]+std::abs(q[i])+1e-30);if(a< -tol){++s.negative_before;s.deficit_before-=a;}if(b< -tol){++s.negative_after;s.deficit_after-=b;}}
  s.max_force_residual=std::max(s.max_force_residual,std::hypot(force_sum.x-R.x,force_sum.y-R.y)/std::max(normscale,1e-300));
  s.max_work_residual=std::max(s.max_work_residual,std::abs(work-(vh^R))/std::max(normscale*std::hypot(vh.x,vh.y),1e-300));
  s.max_sum_error=std::max(s.max_sum_error,std::abs(sum-1));
 }
 return forces;
}
inline void finish(p4est_t*f){
 if(!mode())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;auto&s=stats();
 std::printf("RELAX_VECTOR step=%d t=%.17g mode=%d nodes=%lld infeasible=%lld candidates=%lld applied=%lld max_force_residual=%.17g max_work_residual=%.17g\n",r.current_step,r.current_time,mode(),s.nodes,s.vector_infeasible,s.vector_candidates,s.vector_applied,s.max_force_residual,s.max_work_residual);
 long long bad=0,cells=0;double ordinary=0,correction=0,deficit=0,worst=0;int wx=0,wy=0,wl=0;double wr=0,wp=0,wrho=0;
 for(p4est_topidx_t ti=f->first_local_tree;ti<=f->last_local_tree;++ti){auto*t=p4est_tree_array_index(f->trees,ti);for(size_t k=0;k<t->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&t->quadrants,k);const auto*data=static_cast<quad_data_t*>(q->p.user_data);const auto&v=data->m_vara;const auto uc=v.cell_vector(idCentroidVelo_cur);double D=0,Q=0;
  for(int c=0;c<4;++c){const auto du=v.corner_vector(idcnVelocity_lag,c)-uc;D+=du^GeometryAlg::MatrixDotVector(v.MarCnData[idcnMcp][c],du);Q-=du^v.corner_vector(idcnFluxRelaxed,c);
   const auto&pc=data->m_pc_edge_data[c];if(pc.IsParentChildBoun){const auto dp=pc.Hanging_velocity-uc;D+=dp^GeometryAlg::MatrixDotVector(v.MarCnData[ideMcp][c],dp);Q-=dp^pc.FluxRelaxed;}}
  ++cells;ordinary+=D;correction+=Q;const double net=D+Q;if(net< -1e-12*(std::abs(D)+std::abs(Q)+1e-30)){++bad;deficit-=net;}
  if(net<worst){worst=net;wx=q->x;wy=q->y;wl=q->level;const auto x=v.cell_vector(idCentroidCoord_cur);wr=std::hypot(x.x,x.y);wp=v.cell(idPressure_cur);wrho=v.cell(idDensity_cur);}
 }}
 std::printf("RELAX_DISS step=%d t=%.17g mode=%d nodes=%lld infeasible=%lld candidates=%lld applied=%lld negative_before=%lld negative_after=%lld deficit_before=%.17g deficit_after=%.17g max_sum_error=%.17g cells=%lld negative_cells=%lld ordinary=%.17g correction=%.17g cell_deficit=%.17g worst=%.17g worst_x=%d worst_y=%d worst_level=%d worst_radius=%.17g worst_p=%.17g worst_rho=%.17g\n",r.current_step,r.current_time,mode(),s.nodes,s.infeasible,s.candidates,s.applied,s.negative_before,s.negative_after,s.deficit_before,s.deficit_after,s.max_sum_error,cells,bad,ordinary,correction,deficit,worst,wx,wy,wl,wr,wp,wrho);
}
}
