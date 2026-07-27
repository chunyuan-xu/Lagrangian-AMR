#pragma once
#include <p4est.h>
#include <p4est_vtk.h>
#include "defines.h"
#include "variable.h"


namespace IOAlgorithm {


inline void WriteVTKSolution(p4est_t *p4est, const char *prefix)
{
	p4est_vtk_write_file(p4est, NULL, prefix);
}

void p4est_debug_output_vtu(p4est_t *p4est, const char *prefix, int step, int location_id);

}
