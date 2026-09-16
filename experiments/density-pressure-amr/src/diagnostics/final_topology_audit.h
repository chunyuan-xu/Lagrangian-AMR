#pragma once
#include <cstring>
#include <vector>
#include "amr/coarsen_plan_experiment.h"
#include "defines.h"
#include "variable.h"
#include "diagnostics/minimal_front_topology.h"
namespace FinalTopologyAudit {
using Key=CoarsenPlanExperiment::Key;
inline std::set<Key>&targets(){static std::set<Key>s;return s;}
inline double radius(const CVariable&v){const int o[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};double a=0,x=0,y=0;for(int k=0;k<4;++k){auto p=v.corner_vector(idcnCoords_cur,o[k]),q=v.corner_vector(idcnCoords_cur,o[(k+1)%4]);double c=p.x*q.y-p.y*q.x;a+=c;x+=(p.x+q.x)*c;y+=(p.y+q.y)*c;}SC_CHECK_ABORT(a>0.,"Invalid final audit area");return std::hypot(x/(3*a),y/(3*a));}
inline int force_far_family(p4est_t*,p4est_topidx_t t,p4est_quadrant_t**q){p4est_quadrant_t p;p4est_quadrant_parent(q[0],&p);return targets().count(CoarsenPlanExperiment::key(t,&p))?1:0;}
inline void record(p4est_t*f){
 if(!std::getenv("AMR_FINAL_TOPOLOGY_AUDIT"))return;
 SC_CHECK_ABORT(f->mpisize==1&&f->connectivity->num_trees==1,"Final necessity diagnostic currently single-rank single-tree only");
 const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;SC_CHECK_ABORT(r.which_case==ProblemNo::NohCartesian,"Noh-only analytic diagnostic");
 // Analytic radius selects only a disposable topology-copy experiment.
 // It never enters actual refinement/coarsening callbacks or fluid updates.
 std::map<Key,p4est_quadrant_t*> leaves;std::map<Key,std::vector<unsigned char>> snapshots;targets().clear();
 for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tr=p4est_tree_array_index(f->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);auto key=CoarsenPlanExperiment::key(t,q);leaves[key]=q;auto&b=snapshots[key];b.resize(sizeof(quad_data_t));std::memcpy(b.data(),q->p.user_data,b.size());auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;if(q->level>=6&&std::abs(radius(v)-r.current_time/3.)>.04){p4est_quadrant_t p;p4est_quadrant_parent(q,&p);targets().insert(CoarsenPlanExperiment::key(t,&p));}}}
 // Terminal-only, read-only export. Cached sensors are not fresh decisions.
 if(std::getenv("AMR_FRONT_DEPENDENCY_AUDIT")) {
  const int order[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};
  for(const auto&entry:leaves) {
   const auto*q=entry.second;const auto&v=static_cast<const quad_data_t*>(q->p.user_data)->m_vara;
   const auto rr=MinimalFrontTopology::radial_range(v);
   std::printf("DEPENDENCY_LEAF step=%d t=%.17g tree=%d level=%d x=%lld y=%lld radius=%.17g rmin=%.17g rmax=%.17g rho=%.17g p=%.17g area=%.17g gradient=%.17g jp_cached=%.17g allow_cached=%d",
    r.current_step,r.current_time,(int)std::get<0>(entry.first),(int)q->level,(long long)q->x,(long long)q->y,radius(v),rr.first,rr.second,v.cell(idDensity_cur),v.cell(idPressure_cur),v.cell(idVolume),v.cell(idCDensityGradient),v.cell(idAMRPressureJump),v.int_cell(idAllowCoarsening));
   for(int c=0;c<4;++c){const auto p=v.corner_vector(idcnCoords_cur,order[c]);std::printf(" px%d=%.17g py%d=%.17g",c,p.x,c,p.y);}
   std::printf("\n");
  }
 }
 CoarsenPlanExperiment::Plan plan;CoarsenPlanExperiment::prepare(f,force_far_family,plan);
 int blocked=0,nonfamily=0,removable=0;
 for(auto&key:targets()){bool available=plan.requested.count(key),veto=plan.blocked.count(key);blocked+=veto;nonfamily+=!available;removable+=available&&!veto;int lev=std::get<1>(key);long long x=std::get<2>(key),y=std::get<3>(key),h=P4EST_QUADRANT_LEN(lev);int fine=0;double rmin=1e30,rmax=0.,pmin=1e30,pmax=0.;
 for(auto&l:leaves){auto*q=l.second;long long hh=P4EST_QUADRANT_LEN(q->level);if(q->level>lev+1&&q->x<=x+h&&q->x+hh>=x&&q->y<=y+h&&q->y+hh>=y){++fine;auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;double rad=radius(v);rmin=std::min(rmin,rad);rmax=std::max(rmax,rad);pmin=std::min(pmin,v.cell(idPressure_cur));pmax=std::max(pmax,v.cell(idPressure_cur));}}
 P4EST_GLOBAL_PRODUCTIONF("NECESSITY level=%d x=%lld y=%lld family=%d blocked=%d fine_contacts=%d fine_rmin=%.17g fine_rmax=%.17g fine_pmin=%.17g fine_pmax=%.17g\n",lev,x,y,available,veto,fine,rmin,rmax,pmin,pmax);}
 std::set<Key> far_leaves;for(auto&l:leaves){auto&q=*l.second;const auto&v=static_cast<const quad_data_t*>(q.p.user_data)->m_vara;if(q.level>=6&&std::abs(radius(v)-r.current_time/3.)>.04)far_leaves.insert(l.first);}
 MinimalFrontTopology::audit(f,leaves,far_leaves);
 for(auto&l:leaves)SC_CHECK_ABORT(std::memcmp(snapshots[l.first].data(),l.second->p.user_data,sizeof(quad_data_t))==0,"Read-only topology diagnostic mutated fluid state");
 P4EST_GLOBAL_PRODUCTIONF("NECESSITY_SUMMARY step=%d t=%.17g parents=%d blocked=%d nonfamily=%d removable=%d\n",r.current_step,r.current_time,(int)targets().size(),blocked,nonfamily,removable);
}
}
