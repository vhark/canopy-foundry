#pragma once

#include <canopy/clock.hpp>

#include <span>

namespace canopy {
struct ControlView {
    Id id;
    Fraction requested;
    Fraction applied;
    friend bool operator==(const ControlView&, const ControlView&) = default;
};
// Event order is (second, phase, entity ID, command ID). A decision blocks further ticks.
enum class EventType : std::uint8_t {
    ControlRequested, ControlApplied, ClimateBoundary, CropBoundary,
    DecisionRequired, DecisionResolved
};
struct Event {
    SimSecond second;
    EventType type;
    Id entity;
    Id command_id;
    Fraction value;
    friend bool operator==(const Event&, const Event&) = default;
};
struct WorldView {
    SimSecond second;
    SimSecond next_decision_second;
    bool decision_pending;
    std::uint64_t revision;
    std::uint64_t climate_boundaries;
    std::uint64_t crop_boundaries;
    std::span<const ControlView> controls;
};
// A span remains valid until the next mutating World operation; it never grants mutable access.
} // namespace canopy
