#pragma once

#include <algorithm>
#include <limits>

namespace TimestepReduction {

inline double initial_local_minimum()
{
    return std::numeric_limits<double>::infinity();
}

inline double accumulate_local_minimum(double current_minimum, double cell_timestep)
{
    return std::min(current_minimum, cell_timestep);
}

} // namespace TimestepReduction
