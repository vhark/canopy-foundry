#include <canopy/world.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace canopy {
namespace {
std::size_t receipt_hash(Id id) noexcept {
    std::uint64_t value = id.high ^ id.low;
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    return static_cast<std::size_t>(value);
}
} // namespace
SubmitResult World::submit(const Command& command) noexcept {
    if (!loaded_) return {SubmitError::InvalidModel, {}};
    const auto& header = command.header;
    if (header.command_id == invalid_id) return {SubmitError::InvalidId, {}};
    // The bounded preallocated index never evicts a claim. Retry envelope time and
    // revision are intentionally excluded; actor and typed action are immutable.
    std::size_t slot = receipt_hash(header.command_id) % receipt_index_.size();
    while (receipt_index_[slot] != 0) {
        const std::size_t index = receipt_index_[slot] - 1;
        if (commands_[index].header.command_id == header.command_id) {
            const auto& accepted = commands_[index];
            return accepted.header.actor_id == header.actor_id && accepted.action == command.action
                ? SubmitResult{SubmitError::None, receipts_[index]}
                : SubmitResult{SubmitError::Conflict, {}};
        }
        slot = (slot + 1 == receipt_index_.size()) ? 0 : slot + 1;
    }
    if (header.actor_id == invalid_id) return {SubmitError::InvalidId, {}};
    if (header.issued_at != second_) return {SubmitError::InvalidTime, {}};
    if (header.expected_revision != revision_) return {SubmitError::StaleRevision, {}};
    if (revision_ == std::numeric_limits<std::uint64_t>::max()) return {SubmitError::InvalidModel, {}};

    ControlView* control = nullptr;
    Event event{second_, EventType::DecisionResolved, {}, header.command_id, {}};
    if (const auto* action = std::get_if<SetControl>(&command.action)) {
        if (action->entity_id == invalid_id) return {SubmitError::InvalidId, {}};
        if (!std::isfinite(action->requested.value) || action->requested.value < 0.0 ||
            action->requested.value > 1.0) return {SubmitError::InvalidValue, {}};
        const auto found = std::lower_bound(controls_.begin(), controls_.end(), action->entity_id,
            [](const ControlView& candidate, Id id) { return candidate.id < id; });
        if (found == controls_.end() || found->id != action->entity_id)
            return {SubmitError::UnknownEntity, {}};
        control = &*found;
        event = {second_, EventType::ControlRequested, action->entity_id,
                 header.command_id, action->requested};
    } else {
        const auto& resolution = std::get<ResolveDecision>(command.action);
        if (!decision_pending_ || decision_index_ >= deadlines_.size() ||
            resolution.decision_second != second_ ||
            resolution.decision_id != deadlines_[decision_index_].id)
            return {SubmitError::InvalidDecision, {}};
        event.entity = deadlines_[decision_index_].entity_id;
    }
    if (receipts_.size() == limits_.max_receipts || events_.size() == limits_.max_events)
        return {SubmitError::CapacityExceeded, {}};

    const Receipt receipt{header.command_id, revision_ + 1, second_};
    commands_.push_back(command);
    receipts_.push_back(receipt);
    receipt_index_[slot] = commands_.size();
    insert_event(event);
    revision_ = receipt.revision;
    if (control != nullptr) {
        control->requested = std::get<SetControl>(command.action).requested;
    } else {
        ++decision_index_;
        decision_pending_ = decision_index_ < deadlines_.size() &&
                            deadlines_[decision_index_].second == second_;
    }
    return {SubmitError::None, receipt};
}
} // namespace canopy
