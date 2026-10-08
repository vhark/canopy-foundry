#pragma once

#include <canopy/world.hpp>

#include <array>
#include <exception>

namespace canopy::test {
inline constexpr Id room{0, 7};
inline constexpr Id actor{0, 9};
inline constexpr Id first_command{0x0000000000004000ULL, 0x8000000000000001ULL};

inline World make_world(WorldLimits limits = {2, 32, 32}) {
    World world(limits);
    constexpr std::array controls{InitialControl{room, Fraction{0.25}}};
    constexpr std::array deadlines{
        DecisionDeadline{{0, 60}, room, 60},
        DecisionDeadline{{0, 120}, room, 120}
    };
    if (world.load(controls, deadlines, 41) != LoadError::None) {
        std::terminate();
    }
    return world;
}

inline Command control(Id id = first_command, double fraction = 0.75,
                       std::uint64_t revision = 0, SimSecond at = 0,
                       Id entity = room) {
    return {CommandHeader{id, actor, revision, at}, SetControl{entity, Fraction{fraction}}};
}

inline Command resolve(Id id, std::uint64_t revision, SimSecond at, Id decision_id = {0, 60}) {
    return {CommandHeader{id, actor, revision, at}, ResolveDecision{decision_id, at}};
}
} // namespace canopy::test
