#pragma once
#include <canopy/world.hpp>

namespace canopy {
// Owned copy, captured only by the unique domain owner between mutating operations.
struct WorldState {
    WorldLimits limits{};
    SimSecond second{};
    std::size_t decision_index{};
    bool decision_pending{};
    std::uint64_t revision{}, seed{}, climate_boundaries{}, crop_boundaries{};
    std::vector<InitialControl> initial_controls;
    std::vector<DecisionDeadline> deadlines;
    std::vector<ControlView> controls;
    std::vector<Command> commands;
    std::vector<Receipt> receipts;
    std::vector<Event> events;
};
} // namespace canopy
