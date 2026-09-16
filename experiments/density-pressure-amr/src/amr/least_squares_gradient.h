#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

// Weighted cell-centre fit: min sum_j ((dr_j . g - delta_rho_j)/|dr_j|)^2.
// Streaming two-column QR avoids squaring the stencil condition number.
// No p4est dependency: this kernel is also used by the manufactured-field tests.
namespace DensityLeastSquares {
struct Result {
    double x = 0., y = 0.;
    double magnitude = std::numeric_limits<double>::infinity();
    double reciprocal_condition = 0.;
    bool valid = false;
};

class Fit {
    long double r11_ = 0., r12_ = 0., r22_ = 0., z1_ = 0., z2_ = 0.;
    bool invalid_ = false;
    int samples_ = 0;
public:
    void invalidate() { invalid_ = true; }
    void add(double dx, double dy, double difference) {
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(difference)) {
            invalidate(); return;
        }
        const long double distance = std::hypot((long double)dx, (long double)dy);
        if (!(distance > 0.)) { invalidate(); return; }
        long double a = dx / distance, b = dy / distance, v = difference / distance;
        const long double first = std::hypot(r11_, a);
        if (first > 0.) {
            const long double c = r11_ / first, s = a / first;
            const long double next_b = -s * r12_ + c * b;
            const long double next_v = -s * z1_ + c * v;
            r12_ = c * r12_ + s * b;
            z1_ = c * z1_ + s * v;
            r11_ = first; b = next_b; v = next_v;
        }
        const long double second = std::hypot(r22_, b);
        if (second > 0.) {
            z2_ = (r22_ / second) * z2_ + (b / second) * v;
            r22_ = second;
        }
        ++samples_;
    }
    Result solve() const {
        Result result;
        if (invalid_ || samples_ < 2) return result;
        // Singular values of a 2x2 upper triangular R. The determinant/large
        // eigenvalue expression avoids cancellation in the smaller value.
        const long double a = r11_*r11_, b = r11_*r12_;
        const long double d = r12_*r12_ + r22_*r22_;
        const long double largest2 = .5L*(a+d+std::hypot(a-d,2*b));
        if (!(largest2 > 0.)) return result;
        const long double rcond = std::abs(r11_*r22_) / largest2;
        result.reciprocal_condition = (double)rcond;
        // Unresolvable directions must not be interpreted as a smooth cell.
        if (!(rcond > 1e-10L)) return result;
        const long double gy = z2_ / r22_;
        const long double gx = (z1_ - r12_*gy) / r11_;
        result.x = (double)gx; result.y = (double)gy;
        result.magnitude = std::hypot(result.x, result.y);
        result.valid = std::isfinite(result.magnitude);
        if (!result.valid) result.magnitude = std::numeric_limits<double>::infinity();
        return result;
    }
};
} // namespace DensityLeastSquares
