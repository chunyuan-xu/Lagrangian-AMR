#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace ShockFrontPolicy {

struct Trajectory {
    double radius_scale;
    double radius_exponent;
};

struct RadialBounds {
    double minimum;
    double maximum;
};

inline double constant_speed_radius(double current_time, double shock_speed)
{
    return shock_speed * std::max(0.0, current_time);
}

inline double power_law_radius(
    double current_time, double radius_scale, double radius_exponent)
{
    return radius_scale *
        std::pow(std::max(0.0, current_time), radius_exponent);
}

inline double sedov_radius(double current_time, double radius_scale)
{
    return power_law_radius(current_time, radius_scale, 0.5);
}

inline double radius_at_time(double current_time, const Trajectory& trajectory)
{
    return power_law_radius(
        current_time, trajectory.radius_scale, trajectory.radius_exponent);
}

inline RadialBounds radial_bounds(const std::array<std::array<double, 2>, 4>& corners)
{
    double center_x = 0.0;
    double center_y = 0.0;
    for (const std::array<double, 2>& corner : corners) {
        center_x += corner[0];
        center_y += corner[1];
    }
    center_x /= corners.size();
    center_y /= corners.size();

    double enclosing_radius = 0.0;
    for (const std::array<double, 2>& corner : corners) {
        const double dx = corner[0] - center_x;
        const double dy = corner[1] - center_y;
        enclosing_radius = std::max(enclosing_radius, std::sqrt(dx * dx + dy * dy));
    }

    const double center_radius = std::sqrt(center_x * center_x + center_y * center_y);
    return RadialBounds{
        std::max(0.0, center_radius - enclosing_radius),
        center_radius + enclosing_radius};
}

inline bool intersects_radial_band(
    const RadialBounds& bounds, double front_radius, double half_width)
{
    if (!std::isfinite(bounds.minimum) || !std::isfinite(bounds.maximum) ||
        !std::isfinite(front_radius) || !std::isfinite(half_width) ||
        half_width < 0.0) {
        return true;
    }

    const double inner_radius = std::max(0.0, front_radius - half_width);
    const double outer_radius = front_radius + half_width;
    return bounds.maximum >= inner_radius && bounds.minimum <= outer_radius;
}

} // namespace ShockFrontPolicy
