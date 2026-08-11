#pragma once
#include <p4est.h>

// M7.4: Simulation — high-level orchestration. main.cpp only performs
// MPI/p4est/config/mesh setup, then forwards to Simulation::run with a
// caller-supplied time-step driver (the full hydro+AMR loop). This keeps the
// module boundary without moving the large driver body, which stays byte
// identical.

namespace Simulation {

typedef void (*driver_fn)(p4est_t *, double, double);

inline void run(p4est_t *p4est, double start_time, double end_time,
	driver_fn advance)
{
	advance(p4est, start_time, end_time);
}

} // namespace Simulation
