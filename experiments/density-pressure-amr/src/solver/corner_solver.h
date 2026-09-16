#pragma once
#include <p4est.h>
#include <p4est_ghost.h>
#include "defines.h"
#include "variable.h"

namespace SolverAlgorithm {


void MatrixAssemble(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data);


void ComputeCornerNodeVelocity(p4est_t *p4est, p4est_ghost_t *ghost, void *ghost_data);

} 
