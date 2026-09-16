#pragma once
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <p4est_bits.h>
#include "defines.h"
#include "variable.h"
#include "amr/pressure_experiment.h"
#include "amr/compression_experiment.h"
// Isolated topology-priority experiment. No physical coordinates, radius,
// reference solution or diagnostic distance enters this decision.
// Mode 1 audit, 2 veto new L7 and prefer merging existing L7,
// mode 3 veto new L7 only (normal coarsening is left unchanged).
namespace CoarsePriorityExperiment {
using Key=std::tuple<int,int,int>;
inline int mode(){static const int m=std::getenv("AMR_COARSE_PRIORITY")?std::atoi(std::getenv("AMR_COARSE_PRIORITY")):0;return m;}
inline Key key(const p4est_quadrant_t&q){return Key(q.level,q.x,q.y);}
struct State {bool active=false;std::vector<p4est_quadrant_t> coarse;std::set<Key> refined,coarsened;};
inline State&state(){static State s;return s;}
inline bool smooth(const CVariable&v,const p4est_data_t&r,double(*indicator)(const CVariable&)){
 const double e=indicator(v),p=v.cell(idAMRPressureJump),c=CompressionExperiment::indicator(v);
 return std::isfinite(e)&&std::isfinite(p)&&std::isfinite(c)&&e<r.coarsen_error&&p<PressureExperiment::coarsen()&&
  (!CompressionExperiment::runtime_enabled()||c<.5*CompressionExperiment::threshold());
}
inline void prepare(p4est_t*f,double(*indicator)(const CVariable&)){
 if(!mode())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
 SC_CHECK_ABORT(f->mpisize==1&&f->connectivity->num_trees==1&&r.max_level==7&&r.minus_level==4,
  "Coarse-priority prototype requires single-rank/single-tree L4-L7");
 auto&s=state();s=State{};s.active=true;
 struct Group {int mask=0;bool all=true;p4est_quadrant_t parent;};std::map<Key,Group> groups;
 auto*t=p4est_tree_array_index(f->trees,0);
 for(size_t i=0;i<t->quadrants.elem_count;++i){auto*q=p4est_quadrant_array_index(&t->quadrants,i);const auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;
  const bool good=smooth(v,r,indicator);
  if(q->level==5&&good)s.coarse.push_back(*q);
  if(q->level==6){p4est_quadrant_t p;p4est_quadrant_parent(q,&p);auto&g=groups[key(p)];g.parent=p;g.all=g.all&&good;g.mask|=1<<p4est_quadrant_child_id(q);}
 }
 for(const auto&g:groups)if(g.second.mask==15&&g.second.all)s.coarse.push_back(g.second.parent);
}
inline bool touches(const p4est_quadrant_t&q){
 auto&s=state();if(!s.active||q.level!=6)return false;const long long h=P4EST_QUADRANT_LEN(q.level);
 for(const auto&p:s.coarse){const long long H=P4EST_QUADRANT_LEN(p.level);
  if((long long)q.x<=p.x+H&&(long long)p.x<=q.x+h&&(long long)q.y<=p.y+H&&(long long)p.y<=q.y+h)return true;
 }return false;
}
inline bool veto_refine(const p4est_quadrant_t&q){
 if(!mode()||!touches(q))return false;state().refined.insert(key(q));return mode()==2||mode()==3;
}
inline bool prefer_coarsen(p4est_quadrant_t**qs){
 if(!mode()||qs[0]->level!=7)return false;p4est_quadrant_t p;p4est_quadrant_parent(qs[0],&p);
 if(!touches(p))return false;
 // The caller has already checked the original geometric safety tags and
 // finite density indicator. Also retain active compression and invalid p.
 for(int i=0;i<4;++i){const auto&v=static_cast<quad_data_t*>(qs[i]->p.user_data)->m_vara;
  if(!std::isfinite(v.cell(idAMRPressureJump)))return false;
  const double c=CompressionExperiment::indicator(v);
  if(!std::isfinite(c)||(CompressionExperiment::runtime_enabled()&&c>=.5*CompressionExperiment::threshold()))return false;
 }
 state().coarsened.insert(key(p));return mode()==2;
}
inline void finish(p4est_t*f){if(!mode())return;auto&s=state();const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
 P4EST_GLOBAL_PRODUCTIONF("COARSE_PRIORITY step=%d t=%.17g mode=%d smooth_regions=%d refine_candidates=%d coarsen_candidates=%d\n",r.current_step,r.current_time,mode(),(int)s.coarse.size(),(int)s.refined.size(),(int)s.coarsened.size());s.active=false;
}
}
