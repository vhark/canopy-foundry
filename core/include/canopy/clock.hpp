#pragma once

#include <canopy/command.hpp>

#include <cstdint>

namespace canopy {
struct AdvanceBudget {
    SimSecond target_second;
    std::uint32_t max_substeps;
};
enum class AdvanceStop { ReachedTarget, BudgetExhausted, DecisionRequired, InvalidModel, CapacityExhausted };
struct AdvanceResult {
    SimSecond reached_second;
    AdvanceStop stop;
    friend bool operator==(const AdvanceResult&, const AdvanceResult&) = default;
};

// Intervals divide nonnegative simulation seconds. Arithmetic never rounds the target.
[[nodiscard]] bool is_boundary(SimSecond second, SimSecond interval) noexcept;
// Counter-based, stream-separated sample, stable for a seed/entity/counter on one build.
[[nodiscard]] std::uint64_t random_sample(std::uint64_t seed, Id entity,
                                          std::uint64_t stream, std::uint64_t counter) noexcept;
} // namespace canopy
