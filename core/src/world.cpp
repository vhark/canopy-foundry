#include <canopy/world.hpp>
#include <canopy/world_state.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace canopy {
namespace {
bool valid(Fraction fraction) noexcept {
    return std::isfinite(fraction.value) && fraction.value >= 0.0 && fraction.value <= 1.0;
}
} // namespace

World::World(World&& other) noexcept : World(other.limits_) {
    swap(other);
}

World& World::operator=(World&& other) noexcept {
    if (this != &other) {
        World replacement(std::move(other));
        swap(replacement);
    }
    return *this;
}

void World::swap(World& other) noexcept {
    using std::swap;
    swap(limits_, other.limits_);
    swap(loaded_, other.loaded_);
    swap(second_, other.second_);
    swap(decision_index_, other.decision_index_);
    swap(decision_pending_, other.decision_pending_);
    swap(revision_, other.revision_);
    swap(seed_, other.seed_);
    swap(climate_boundaries_, other.climate_boundaries_);
    swap(crop_boundaries_, other.crop_boundaries_);
    swap(deadlines_, other.deadlines_);
    swap(initial_controls_, other.initial_controls_);
    swap(controls_, other.controls_);
    swap(commands_, other.commands_);
    swap(receipts_, other.receipts_);
    swap(receipt_index_, other.receipt_index_);
    swap(events_, other.events_);
}

LoadError World::load(std::span<const InitialControl> controls,
                      std::span<const DecisionDeadline> deadlines, std::uint64_t seed) {
    if (loaded_) return LoadError::AlreadyLoaded;
    if (controls.empty() || controls.size() > limits_.max_entities ||
        deadlines.size() > limits_.max_decisions ||
        limits_.max_receipts == 0 || limits_.max_events == 0 ||
        limits_.max_entities > controls_.max_size() ||
        limits_.max_decisions > deadlines_.max_size() ||
        limits_.max_receipts > commands_.max_size() ||
        limits_.max_receipts > receipts_.max_size() ||
        limits_.max_receipts > (receipt_index_.max_size() - 1) / 2 ||
        limits_.max_events > events_.max_size()) return LoadError::CapacityExceeded;
    for (std::size_t i = 0; i < controls.size(); ++i) {
        if (controls[i].id == invalid_id) return LoadError::InvalidId;
        if (!valid(controls[i].initial)) return LoadError::InvalidValue;
        for (std::size_t j = 0; j < i; ++j) {
            if (controls[i].id == controls[j].id) return LoadError::DuplicateEntity;
        }
    }
    for (std::size_t i = 0; i < deadlines.size(); ++i) {
        if (deadlines[i].id == invalid_id || deadlines[i].entity_id == invalid_id)
            return LoadError::InvalidId;
        if (deadlines[i].second <= 0) return LoadError::InvalidTime;
        if (std::none_of(controls.begin(), controls.end(), [&](const InitialControl& c) {
                return c.id == deadlines[i].entity_id;
            })) return LoadError::UnknownEntity;
        for (std::size_t j = 0; j < i; ++j) {
            if (deadlines[i].id == deadlines[j].id) return LoadError::DuplicateDecision;
        }
    }
    initial_controls_.reserve(limits_.max_entities);
    controls_.reserve(limits_.max_entities);
    deadlines_.reserve(limits_.max_decisions);
    commands_.reserve(limits_.max_receipts);
    receipts_.reserve(limits_.max_receipts);
    receipt_index_.resize(limits_.max_receipts * 2 + 1, 0);
    events_.reserve(limits_.max_events);
    for (const auto& control : controls) {
        controls_.push_back({control.id, control.initial, control.initial});
    }
    initial_controls_.assign(controls.begin(), controls.end());
    std::sort(controls_.begin(), controls_.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    deadlines_.assign(deadlines.begin(), deadlines.end());
    std::sort(deadlines_.begin(), deadlines_.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second < b.second;
        if (a.entity_id != b.entity_id) return a.entity_id < b.entity_id;
        return a.id < b.id;
    });
    seed_ = seed;
    loaded_ = true;
    return LoadError::None;
}

WorldView World::view() const noexcept {
    const SimSecond next = decision_index_ < deadlines_.size() ? deadlines_[decision_index_].second : -1;
    return {second_, next, decision_pending_, revision_,
            climate_boundaries_, crop_boundaries_, controls_};
}

void World::insert_event(Event event) noexcept {
    const auto before = std::lower_bound(events_.begin(), events_.end(), event,
        [](const Event& a, const Event& b) {
            if (a.second != b.second) return a.second < b.second;
            if (a.type != b.type) return a.type < b.type;
            if (a.entity != b.entity) return a.entity < b.entity;
            return a.command_id < b.command_id;
        });
    events_.insert(before, event);
}

AdvanceResult World::advance(AdvanceBudget budget) noexcept {
    if (!loaded_ || budget.target_second < second_) return {second_, AdvanceStop::InvalidModel};
    if (decision_pending_) return {second_, AdvanceStop::DecisionRequired};
    if (budget.target_second == second_) return {second_, AdvanceStop::ReachedTarget};
    for (std::uint32_t steps = 0; steps < budget.max_substeps; ++steps) {
        if (second_ == std::numeric_limits<SimSecond>::max()) return {second_, AdvanceStop::InvalidModel};
        const SimSecond next = second_ + 1;
        const bool climate = is_boundary(next, 5);
        const bool crop = is_boundary(next, 60);
        const bool decision = decision_index_ < deadlines_.size() &&
                              next == deadlines_[decision_index_].second;
        std::size_t changes = static_cast<std::size_t>(climate) + static_cast<std::size_t>(crop);
        for (const auto& control : controls_) {
            if (control.applied != control.requested) ++changes;
        }
        if (decision) {
            for (std::size_t i = decision_index_;
                 i < deadlines_.size() && deadlines_[i].second == next; ++i) ++changes;
        }
        if (changes > limits_.max_events - events_.size()) return {second_, AdvanceStop::CapacityExhausted};
        // A one-second integration step is atomic: check every admission before altering state.
        second_ = next;
        for (auto& control : controls_) {
            if (control.applied != control.requested) {
                control.applied = control.requested;
                insert_event({next, EventType::ControlApplied, control.id, {}, control.applied});
            }
        }
        if (climate) {
            ++climate_boundaries_;
            insert_event({next, EventType::ClimateBoundary, {}, {}, {}});
        }
        if (crop) {
            ++crop_boundaries_;
            insert_event({next, EventType::CropBoundary, {}, {}, {}});
        }
        if (decision) {
            decision_pending_ = true;
            for (std::size_t i = decision_index_;
                 i < deadlines_.size() && deadlines_[i].second == next; ++i) {
                insert_event({next, EventType::DecisionRequired,
                              deadlines_[i].entity_id, deadlines_[i].id, {}});
            }
            return {second_, AdvanceStop::DecisionRequired};
        }
        if (second_ == budget.target_second) return {second_, AdvanceStop::ReachedTarget};
    }
    return {second_, AdvanceStop::BudgetExhausted};
}
} // namespace canopy
