#pragma once
#include <set>
#include <map>
#include "amr/coarsen_plan_experiment.h"
#include "defines.h"
#include "variable.h"
// Disposable topology diagnostic only; analytic reference is never an AMR input.
namespace MinimalFrontTopology {
using Key=CoarsenPlanExperiment::Key;
struct Context {int minimum;std::set<Key> protected_ancestors;};
inline std::pair<double,double> radial_range(const CVariable&v){
 const int order[4]={0,3,2,1};double lo=1e30,hi=0.;bool inside=false;
 for(int k=0,j=3;k<4;j=k++){auto a=v.corner_vector(idcnCoords_cur,order[k]),b=v.corner_vector(idcnCoords_cur,order[j]);double dx=b.x-a.x,dy=b.y-a.y;const double den=dx*dx+dy*dy;SC_CHECK_ABORT(den>0.,"Invalid edge in front audit");double s=std::max(0.,std::min(1.,-(a.x*dx+a.y*dy)/den));lo=std::min(lo,std::hypot(a.x+s*dx,a.y+s*dy));hi=std::max(hi,std::hypot(a.x,a.y));if((a.y>0.)!=(b.y>0.)){double cross=a.x+(b.x-a.x)*(-a.y)/(b.y-a.y);if(cross>0.)inside=!inside;}}
 if(inside)lo=0.;return {lo,hi};
}
inline int coarsen(p4est_t*f,p4est_topidx_t t,p4est_quadrant_t**qs){auto&c=*static_cast<Context*>(f->user_pointer);if(qs[0]->level<=c.minimum)return 0;p4est_quadrant_t p;p4est_quadrant_parent(qs[0],&p);return c.protected_ancestors.count(CoarsenPlanExperiment::key(t,&p))?0:1;}
inline bool face_neighbor(const p4est_quadrant_t*a,const p4est_quadrant_t*b){long long ha=P4EST_QUADRANT_LEN(a->level),hb=P4EST_QUADRANT_LEN(b->level);return (((long long)a->x+ha==b->x||(long long)b->x+hb==a->x)&&std::max((long long)a->y,(long long)b->y)<std::min(a->y+ha,b->y+hb))||(((long long)a->y+ha==b->y||(long long)b->y+hb==a->y)&&std::max((long long)a->x,(long long)b->x)<std::min(a->x+ha,b->x+hb));}
inline void audit(p4est_t*f,const std::map<Key,p4est_quadrant_t*>&leaves,const std::set<Key>&far){
 if(!std::getenv("AMR_MINIMAL_FRONT_AUDIT"))return;
 const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;SC_CHECK_ABORT(f->mpisize==1&&f->connectivity->num_trees==1&&r.which_case==ProblemNo::NohCartesian,"Minimal front audit is single-rank Noh diagnostic");
 for(int mode=0;mode<3;++mode){Context c;c.minimum=r.minus_level;std::set<Key> locked;
  for(auto&entry:leaves){auto*q=entry.second;if(q->level!=r.max_level)continue;auto&v=static_cast<const quad_data_t*>(q->p.user_data)->m_vara;auto rr=radial_range(v);bool keep=mode==0||(mode==1&&rr.first<=r.current_time/3.&&rr.second>=r.current_time/3.);
   if(mode==2)for(auto&nb:leaves)if(face_neighbor(q,nb.second)){double a=v.cell(idPressure_cur)-8./3.,b=static_cast<const quad_data_t*>(nb.second->p.user_data)->m_vara.cell(idPressure_cur)-8./3.;if(a*b<=0.){keep=true;break;}}
   if(keep){locked.insert(entry.first);p4est_quadrant_t parent=*q;while(parent.level>r.minus_level){p4est_quadrant_t next;p4est_quadrant_parent(&parent,&next);c.protected_ancestors.insert(CoarsenPlanExperiment::key(std::get<0>(entry.first),&next));parent=next;}}
  }
  auto*copy=p4est_copy(f,0);copy->user_pointer=&c;p4est_coarsen_ext(copy,1,0,coarsen,nullptr,nullptr);p4est_balance_ext(copy,P4EST_CONNECT_CORNER,nullptr,nullptr);std::set<Key> actual;int levels[4]={};
  for(p4est_topidx_t t=copy->first_local_tree;t<=copy->last_local_tree;++t){auto*tr=p4est_tree_array_index(copy->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);actual.insert(CoarsenPlanExperiment::key(t,q));SC_CHECK_ABORT(q->level>=4&&q->level<=7,"Unexpected minimal diagnostic level");++levels[q->level-4];}}
  for(auto&k:locked)SC_CHECK_ABORT(actual.count(k),"Minimal topology removed protected front leaf");
  int remaining=0;for(auto&k:far){auto*q=leaves.at(k);bool retained=false;for(auto&a:actual){long long h=P4EST_QUADRANT_LEN(q->level);if(std::get<0>(a)==std::get<0>(k)&&std::get<1>(a)>=q->level&&std::get<2>(a)>=q->x&&std::get<2>(a)<q->x+h&&std::get<3>(a)>=q->y&&std::get<3>(a)<q->y+h){retained=true;break;}}remaining+=retained;}
  P4EST_GLOBAL_PRODUCTIONF("MINIMAL_FRONT mode=%d step=%d t=%.17g lockedL7=%d total=%d L4=%d L5=%d L6=%d L7=%d original_far=%d retained_far=%d\n",mode,r.current_step,r.current_time,(int)locked.size(),(int)actual.size(),levels[0],levels[1],levels[2],levels[3],(int)far.size(),remaining);
  p4est_destroy(copy);
 }
}
}
