#pragma once
// Included inside RiemannIterationExperiment after Sample/cells definitions.
inline void dump_node(p4est_t*f,const std::vector<quad_data_t*>&a,const std::vector<Sample>&before,int iterations,double scale){
    double worst=-1.;size_t wi=0;int wc=0;
    for(size_t i=0;i<a.size();++i)for(int c=0;c<4;++c){const auto u=a[i]->m_vara.corner_vector(idcnVelocity_lag,c);
        const double d=std::hypot(u.x-before[i].node[c].x,u.y-before[i].node[c].y);if(d>worst){worst=d;wi=i;wc=c;}}
    const int dx[4]={0,0,1,1},dy[4]={0,1,1,0};p4est_topidx_t target_tree=-1;p4est_qcoord_t nx=0,ny=0;
    for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tree=p4est_tree_array_index(f->trees,t);
        for(size_t j=0;j<tree->quadrants.elem_count;++j){auto*q=p4est_quadrant_array_index(&tree->quadrants,j);if(q->p.user_data==a[wi]){target_tree=t;nx=q->x+dx[wc]*P4EST_QUADRANT_LEN(q->level);ny=q->y+dy[wc]*P4EST_QUADRANT_LEN(q->level);}}}
    SC_CHECK_ABORT(target_tree>=0,"Worst Riemann node not found");
    SC_CHECK_ABORT(worst>0,"Cannot export a zero residual as an unconverged node");
    const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;const auto u=a[wi]->m_vara.corner_vector(idcnVelocity_lag,wc);
    const auto pos=a[wi]->m_vara.corner_vector(idcnCoords_half,wc);
    const auto M=a[wi]->points[wc].MatrixP;const auto b=a[wi]->points[wc].RHS;
    std::printf("RIEMANN_NODE step=%d t=%.17g iter=%d tree=%d nx=%d ny=%d corner=%d hanging=%d xhalf=%.17g yhalf=%.17g ux=%.17g uy=%.17g fx=%.17g fy=%.17g residual=%.17g scale=%.17g mxx=%.17g mxy=%.17g myx=%.17g myy=%.17g bx=%.17g by=%.17g\n",r.current_step,r.current_time,iterations,int(target_tree),nx,ny,wc,int(a[wi]->points[wc].IsHanging),pos.x,pos.y,before[wi].node[wc].x,before[wi].node[wc].y,u.x,u.y,worst/scale,scale,M.xx,M.xy,M.yx,M.yy,b.x,b.y);
    size_t i=0;for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tree=p4est_tree_array_index(f->trees,t);
        for(size_t j=0;j<tree->quadrants.elem_count;++j,++i){auto*q=p4est_quadrant_array_index(&tree->quadrants,j);if(t!=target_tree)continue;
            for(int c=0;c<4;++c){if(q->x+dx[c]*P4EST_QUADRANT_LEN(q->level)!=nx||q->y+dy[c]*P4EST_QUADRANT_LEN(q->level)!=ny)continue;
                const auto&v=a[i]->m_vara;const auto uc=v.cell_vector(idCentroidVelo_cur);
                const auto&p=a[i]->m_cndata[c].hdata[CHalf_edge_data::cside::plus];const auto&m=a[i]->m_cndata[c].hdata[CHalf_edge_data::cside::minus];
                const auto C=v.MarCnData[idcnMcp][c];const auto rhs=v.corner_vector(idcnRHS,c);
                std::printf("RIEMANN_CONTRIB step=%d tree=%d qx=%d qy=%d level=%d corner=%d ux=%.17g uy=%.17g cx=%.17g cy=%.17g rho=%.17g p=%.17g sound=%.17g zp=%.17g zm=%.17g lp=%.17g lm=%.17g rp=%.17g rm=%.17g npx=%.17g npy=%.17g nmx=%.17g nmy=%.17g bp=%d bm=%d vp=%.17g vm=%.17g mxx=%.17g mxy=%.17g myx=%.17g myy=%.17g bx=%.17g by=%.17g\n",r.current_step,int(t),q->x,q->y,int(q->level),c,before[i].node[c].x,before[i].node[c].y,uc.x,uc.y,v.cell(idDensity_cur),v.cell(idPressure_cur),v.cell(idSoundSpeed),p.Zcp,m.Zcp,p.Lcp,m.Lcp,p.Rcp,m.Rcp,p.Ncp.x,p.Ncp.y,m.Ncp.x,m.Ncp.y,int(p.enumBYD),int(m.enumBYD),p.BYDVal,m.BYDVal,C.xx,C.xy,C.yx,C.yy,rhs.x,rhs.y);
            }
        }
    }
}
