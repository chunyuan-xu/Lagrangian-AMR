#pragma once
#include <map>
#include <set>
#include <tuple>
#include <cstdlib>
#include <p4est_extended.h>
#include <p4est_bits.h>
// Topology-only preview. Never transfers fluid state or uses analytic geometry.
// Mode 1 audits; mode 2 vetoes merges undone by corner balance in the preview.
namespace CoarsenPlanExperiment {
using Key=std::tuple<int,int,int,int>;
using Callback=int (*)(p4est_t*,p4est_topidx_t,p4est_quadrant_t**);
inline int mode(){static int x=std::getenv("AMR_COARSEN_PLAN")?std::atoi(std::getenv("AMR_COARSEN_PLAN")):0;return x;}
inline Key key(p4est_topidx_t t,const p4est_quadrant_t*q){return Key(t,q->level,q->x,q->y);}
struct Plan {
 p4est_t *original=nullptr;Callback criterion=nullptr;
 std::map<Key,p4est_quadrant_t*> leaves;
 std::set<Key> requested,blocked,expected_final;
 int applied_veto=0;
};
inline Plan *&active(){static Plan*p=nullptr;return p;}
inline int preview_coarsen(p4est_t *copy,p4est_topidx_t t,p4est_quadrant_t **qs){
 auto &p=*static_cast<Plan*>(copy->user_pointer);p4est_quadrant_t* real[4];
 for(int c=0;c<4;++c){if(!qs[c])return 0;auto it=p.leaves.find(key(t,qs[c]));SC_CHECK_ABORT(it!=p.leaves.end(),"Preview expected original leaf");real[c]=it->second;}
 const int yes=p.criterion(p.original,t,real);
 if(yes){p4est_quadrant_t parent;p4est_quadrant_parent(qs[0],&parent);p.requested.insert(key(t,&parent));}
 return yes;
}
inline void preview_balance(p4est_t *copy,p4est_topidx_t t,int no,p4est_quadrant_t **out,int ni,p4est_quadrant_t **){
 SC_CHECK_ABORT(no==1&&ni==4,"Unexpected balance replacement");auto&p=*static_cast<Plan*>(copy->user_pointer);
 const auto k=key(t,out[0]);if(p.requested.count(k))p.blocked.insert(k);
}
inline void prepare(p4est_t *forest,Callback criterion,Plan &p){
 p.original=forest;p.criterion=criterion;
 for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t){auto*tr=p4est_tree_array_index(forest->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);p.leaves.emplace(key(t,q),q);}}
 auto *copy=p4est_copy(forest,0);copy->user_pointer=&p;
 p4est_coarsen_ext(copy,0,0,preview_coarsen,nullptr,nullptr);
 p4est_balance_ext(copy,P4EST_CONNECT_CORNER,nullptr,preview_balance);
 for(p4est_topidx_t t=copy->first_local_tree;t<=copy->last_local_tree;++t){auto*tr=p4est_tree_array_index(copy->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k)p.expected_final.insert(key(t,p4est_quadrant_array_index(&tr->quadrants,k)));}
 p4est_destroy(copy);
}
inline void verify_topology(p4est_t *forest,const Plan&p){
 std::set<Key> actual;for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t){auto*tr=p4est_tree_array_index(forest->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k)actual.insert(key(t,p4est_quadrant_array_index(&tr->quadrants,k)));}
 SC_CHECK_ABORT(actual==p.expected_final,"Actual balanced topology differs from preview");
}
inline int guarded_coarsen(p4est_t *forest,p4est_topidx_t t,p4est_quadrant_t **qs){
 auto&p=*active();if(!p.criterion(forest,t,qs))return 0;
 p4est_quadrant_t parent;p4est_quadrant_parent(qs[0],&parent);
 if(p.blocked.count(key(t,&parent))){++p.applied_veto;return 0;}return 1;
}
}
