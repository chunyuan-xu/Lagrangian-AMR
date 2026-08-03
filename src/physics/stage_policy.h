#pragma once

namespace StagePolicy {

inline int stage_count()
{
    return 1;
}

inline double timestep_scale(int stage)
{
    return stage == 0 ? 1.0 : 0.0;
}

inline bool accepts_after_stage(int stage)
{
    return stage == stage_count() - 1;
}

} // namespace StagePolicy
