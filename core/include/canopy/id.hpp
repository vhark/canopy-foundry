#pragma once

#include <compare>
#include <cstdint>

namespace canopy {
struct Id {
    std::uint64_t high{};
    std::uint64_t low{};
    friend constexpr bool operator==(Id, Id) = default;
    friend constexpr auto operator<=>(Id, Id) = default;
};
inline constexpr Id invalid_id{};
} // namespace canopy
