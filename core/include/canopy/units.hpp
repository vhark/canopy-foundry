#pragma once

namespace canopy {

// Converts an area in square feet to square metres. The conversion is linear;
// signed zero and non-finite IEEE values follow ordinary floating-point rules.
[[nodiscard]] double square_feet_to_square_metres(double square_feet) noexcept;

} // namespace canopy
