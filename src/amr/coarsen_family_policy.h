#pragma once

#include <array>
#include <cmath>

namespace AMRCoarsenPolicy {

enum class IndicatorMode {
    Gradient,
    DistanceFromShock
};

struct ChildIndicator {
    int level;
    bool coarsening_allowed;
    double value;
};

struct FamilyPolicy {
    IndicatorMode mode;
    int minimum_level;
    int maximum_level;
    double threshold;
};

inline bool child_satisfies_indicator(
    IndicatorMode mode,
    double value,
    double threshold)
{
    if (!std::isfinite(value) || !std::isfinite(threshold)) {
        return false;
    }

    if (mode == IndicatorMode::DistanceFromShock) {
        return value > threshold;
    }
    return value < threshold;
}

inline bool family_allows_coarsening(
    const std::array<ChildIndicator, 4>& children,
    const FamilyPolicy& policy)
{
    for (const ChildIndicator& child : children) {
        if (!child.coarsening_allowed || child.level <= policy.minimum_level) {
            return false;
        }
    }

    for (const ChildIndicator& child : children) {
        if (child.level > policy.maximum_level) {
            return true;
        }
    }

    for (const ChildIndicator& child : children) {
        if (child_satisfies_indicator(policy.mode, child.value, policy.threshold)) {
            return true;
        }
    }
    return false;
}

} // namespace AMRCoarsenPolicy
