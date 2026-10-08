#include <canopy/units.hpp>

namespace canopy {

double square_feet_to_square_metres(double square_feet) noexcept {
    // International foot: exactly 0.3048 metres; area uses the squared factor.
    return square_feet * 0.09290304;
}

} // namespace canopy
