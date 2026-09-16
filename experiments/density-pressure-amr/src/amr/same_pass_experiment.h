#pragma once
#include <map>
#include <set>
#include <tuple>
#include <cstdlib>
#include <p4est_bits.h>
#include "amr/amr_criteria.h"
// Isolated, nonrecursive AMR experiment: do not merge a family created in
// this same pass. No history beyond one pass and no geometric shock model.
namespace SamePassExperiment {
using Key=std::tuple<int,int,int,int>;
using Callback=int (*)(p4est_t*,p4est_topidx_t,p4est_quadrant_t**);
inline int mode(){static int m=std::getenv("AMR_SAME_PASS")?std::atoi(std::getenv("AMR_SAME_PASS")):0;return m;}
inline Key key(p4est_topidx_t t,const p4est_quadrant_t*q){return Key(t,q->level,q->x,q->y);}
struct Value {double h,gradient,rho,eta,jump,compression;};
inline Value value(const p4est_quadrant_t*q){const auto&v=static_cast<const quad_data_t*>(q->p.user_data)->m_vara;return {AMRAgorithm::CellCharacteristicLength(v),v.cell(idCDensityGradient),v.cell(idDensity_cur),AMRAgorithm::DimensionlessDensityGradientIndicator(v),v.cell(idAMRPressureJump),CompressionExperiment::indicator(v)};}
struct Pass {Callback criterion=nullptr;std::map<Key,Value> before;std::set<Key> logged;};
inline Pass*&active(){static Pass*p=nullptr;return p;}
inline void prepare(p4est_t*f,Callback cb,Pass&p){
 p.criterion=cb;
 for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tr=p4est_tree_array_index(f->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);p.before.emplace(key(t,q),value(q));}}
 active()=&p;
}
inline int coarsen(p4est_t*f,p4est_topidx_t t,p4est_quadrant_t**qs){
 auto&p=*active();if(!p.criterion(f,t,qs))return 0;
 for(int c=0;c<4;++c)SC_CHECK_ABORT(qs[c]!=nullptr,"Same-pass guard requires complete family");
 p4est_quadrant_t parent;p4est_quadrant_parent(qs[0],&parent);const auto k=key(t,&parent);auto it=p.before.find(k);if(it==p.before.end())return 1;
 if(p.logged.insert(k).second){
  const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;const auto&v=it->second;
  std::printf("SAME_PARENT step=%d t=%.17g rank=%d tree=%d level=%d x=%d y=%d h=%.17g grad=%.17g rho=%.17g eta=%.17g jp=%.17g comp=%.17g mode=%d\n",r.current_step,r.current_time,f->mpirank,(int)t,(int)parent.level,parent.x,parent.y,v.h,v.gradient,v.rho,v.eta,v.jump,v.compression,mode());
  for(int c=0;c<4;++c){auto w=value(qs[c]);const auto&cv=static_cast<const quad_data_t*>(qs[c]->p.user_data)->m_vara;std::printf("SAME_CHILD step=%d rank=%d tree=%d parent_level=%d x=%d y=%d child=%d h=%.17g grad=%.17g rho=%.17g eta=%.17g jp=%.17g comp=%.17g tag=%d\n",r.current_step,f->mpirank,(int)t,(int)parent.level,parent.x,parent.y,c,w.h,w.gradient,w.rho,w.eta,w.jump,w.compression,cv.int_cell(idAllowCoarsening));}
 }
 return mode()==2?0:1;
}
}
