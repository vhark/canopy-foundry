#pragma once

#include <canopy/view.hpp>

#include <span>
#include <vector>

namespace canopy {
struct WorldLimits {
    std::size_t max_entities;
    std::size_t max_receipts;
    std::size_t max_events;
    std::size_t max_decisions = 32;
};
struct InitialControl {
    Id id;
    Fraction initial;
};
struct DecisionDeadline {
    Id id;
    Id entity_id;
    SimSecond second;
    friend bool operator==(const DecisionDeadline&, const DecisionDeadline&) = default;
};
enum class LoadError {
    None, AlreadyLoaded, CapacityExceeded, DuplicateEntity,
    DuplicateDecision, UnknownEntity, InvalidId, InvalidTime, InvalidValue
};

struct WorldState;
// One owner mutates this model; load admits all storage before normal operations.
// Rejected commands have no receipt and do not claim an ID; accepted IDs never expire.
class World {
public:
    explicit World(WorldLimits limits) noexcept : limits_(limits) {}
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    // Transfer authority; the source becomes empty and may load a new scenario.
    World(World&& other) noexcept;
    World& operator=(World&& other) noexcept;

    [[nodiscard]] LoadError load(std::span<const InitialControl> controls,
                                 std::span<const DecisionDeadline> deadlines, std::uint64_t seed);
    [[nodiscard]] SubmitResult submit(const Command& command) noexcept;
    [[nodiscard]] AdvanceResult advance(AdvanceBudget budget) noexcept;
    [[nodiscard]] WorldView view() const noexcept;
    [[nodiscard]] std::span<const Event> events() const noexcept { return events_; }
    [[nodiscard]] std::span<const Receipt> receipts() const noexcept { return receipts_; }
    // Stateless domain-separated draw; sampling does not mutate the authoritative clock.
    [[nodiscard]] std::uint64_t sample(Id entity, std::uint64_t stream,
                                       std::uint64_t counter) const noexcept {
        return random_sample(seed_, entity, stream, counter);
    }
    [[nodiscard]] WorldState capture() const;
    // A corrupt state is never installed; the destination remains unchanged.
    [[nodiscard]] static LoadError restore(const WorldState& state, World& destination);

private:
    WorldLimits limits_;
    bool loaded_{};
    SimSecond second_{};
    std::size_t decision_index_{};
    bool decision_pending_{};
    std::uint64_t revision_{};
    std::uint64_t seed_{};
    std::uint64_t climate_boundaries_{};
    std::uint64_t crop_boundaries_{};
    std::vector<DecisionDeadline> deadlines_;
    std::vector<InitialControl> initial_controls_;
    std::vector<ControlView> controls_;
    std::vector<Command> commands_;
    std::vector<Receipt> receipts_;
    // Open-addressed index: zero is empty; otherwise one plus the accepted command index.
    std::vector<std::size_t> receipt_index_;
    std::vector<Event> events_;

    void insert_event(Event event) noexcept;
    void swap(World& other) noexcept;
};
} // namespace canopy
