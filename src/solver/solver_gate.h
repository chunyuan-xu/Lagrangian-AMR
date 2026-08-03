#pragma once

namespace SolverGate {

enum class CoordinateType {
    Plane,
    Cylinder
};

enum class SolverType {
    GridAligned,
    Rotated
};

inline bool is_valid_coordinate_type(int value)
{
    return value == 0 || value == 1;
}

inline bool is_valid_solver_type(int value)
{
    return value == 0 || value == 1;
}

inline CoordinateType coordinate_type_from_legacy(int value)
{
    return value == 1 ? CoordinateType::Cylinder : CoordinateType::Plane;
}

inline SolverType solver_type_from_legacy(int value)
{
    return value == 1 ? SolverType::Rotated : SolverType::GridAligned;
}

inline bool should_run_riemann(
    CoordinateType coordinate_type,
    SolverType solver_type)
{
    (void)solver_type;
    return coordinate_type == CoordinateType::Plane;
}

} // namespace SolverGate
