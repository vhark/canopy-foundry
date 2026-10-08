#include <canopy/world_state.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace canopy {
WorldState World::capture() const {
    return {limits_, second_, decision_index_, decision_pending_, revision_, seed_,
            climate_boundaries_, crop_boundaries_, initial_controls_, deadlines_, controls_,
            commands_, receipts_, events_};
}

LoadError World::restore(const WorldState& s, World& destination) {
    if (destination.loaded_) return LoadError::AlreadyLoaded;
    // Absolute caps are independent of untrusted on-disk capacity declarations.
    if (s.limits.max_entities == 0 || s.limits.max_entities > 4096 ||
        s.limits.max_receipts == 0 || s.limits.max_receipts > 65536 ||
        s.limits.max_events == 0 || s.limits.max_events > 262144 ||
        s.limits.max_decisions > 4096 || s.initial_controls.size() > s.limits.max_entities ||
        s.controls.size() != s.initial_controls.size() ||
        s.deadlines.size() > s.limits.max_decisions ||
        s.commands.size() != s.receipts.size() || s.commands.size() > s.limits.max_receipts ||
        s.events.size() > s.limits.max_events || s.second < 0 ||
        s.decision_index > s.deadlines.size() || s.climate_boundaries != static_cast<std::uint64_t>(s.second / 5) ||
        s.crop_boundaries != static_cast<std::uint64_t>(s.second / 60) ||
        s.revision != s.receipts.size()) return LoadError::InvalidValue;
    World candidate(s.limits);
    auto result = candidate.load(s.initial_controls, s.deadlines, s.seed);
    if (result != LoadError::None) return result;
    if (candidate.deadlines_ != s.deadlines || candidate.controls_.size() != s.controls.size())
        return LoadError::InvalidValue;
    for (std::size_t i = 0; i < s.controls.size(); ++i) {
        const auto& control = s.controls[i];
        if (control.id != candidate.controls_[i].id ||
            !std::isfinite(control.requested.value) || !std::isfinite(control.applied.value) ||
            control.requested.value < 0 || control.requested.value > 1 ||
            control.applied.value < 0 || control.applied.value > 1) return LoadError::InvalidValue;
    }
    const bool expected_pending = s.decision_index < s.deadlines.size() &&
        s.deadlines[s.decision_index].second == s.second;
    if (s.decision_pending != expected_pending) return LoadError::InvalidTime;
    for (std::size_t i = 0; i < s.deadlines.size(); ++i) {
        if (i < s.decision_index && s.deadlines[i].second > s.second) return LoadError::InvalidTime;
        if (i >= s.decision_index && s.deadlines[i].second < s.second) return LoadError::InvalidTime;
    }
    for (std::size_t i = 0; i < s.commands.size(); ++i) {
        const auto& command = s.commands[i];
        const auto& receipt = s.receipts[i];
        if (command.header.command_id == invalid_id || command.header.actor_id == invalid_id ||
            receipt.command_id != command.header.command_id || receipt.revision != i + 1 ||
            command.header.expected_revision != i ||
            receipt.accepted_at != command.header.issued_at || receipt.accepted_at < 0 ||
            receipt.accepted_at > s.second) return LoadError::InvalidValue;
        if (const auto* control = std::get_if<SetControl>(&command.action)) {
            if (!std::isfinite(control->requested.value) || control->requested.value < 0 ||
                control->requested.value > 1 ||
                std::none_of(s.controls.begin(), s.controls.end(), [&](const auto& c) { return c.id == control->entity_id; }))
                return LoadError::InvalidValue;
        } else {
            const auto& decision = std::get<ResolveDecision>(command.action);
            if (decision.decision_second != receipt.accepted_at ||
                std::none_of(s.deadlines.begin(), s.deadlines.end(), [&](const auto& d) {
                    return d.id == decision.decision_id && d.second == decision.decision_second;
                })) return LoadError::InvalidValue;
        }
    }
    for (std::size_t i = 0; i < s.events.size(); ++i) {
        const auto& event = s.events[i];
        if (event.second < 0 || event.second > s.second ||
            static_cast<unsigned>(event.type) > static_cast<unsigned>(EventType::DecisionResolved) ||
            !std::isfinite(event.value.value) || event.value.value < 0 || event.value.value > 1)
            return LoadError::InvalidValue;
        if (i != 0) {
            const auto& previous = s.events[i - 1];
            if (previous.second > event.second ||
                (previous.second == event.second && (previous.type > event.type ||
                (previous.type == event.type && (previous.entity > event.entity ||
                (previous.entity == event.entity && previous.command_id > event.command_id))))))
                return LoadError::InvalidValue;
        }
        if ((event.type == EventType::ClimateBoundary && event.second % 5 != 0) ||
            (event.type == EventType::CropBoundary && event.second % 60 != 0)) return LoadError::InvalidTime;
    }
    // Reconstruct rather than trusting claimed control/receipt/event arrays. This
    // also rebuilds the open-addressed receipt index through the ordinary submit path.
    for (std::size_t i = 0; i < s.commands.size(); ++i) {
        const auto& command = s.commands[i];
        if (command.header.issued_at < candidate.second_) return LoadError::InvalidTime;
        if (command.header.issued_at > candidate.second_) {
            const auto distance = command.header.issued_at - candidate.second_;
            if (distance > static_cast<SimSecond>(s.limits.max_events) * 5 + 5)
                return LoadError::InvalidTime;
            const auto progress = candidate.advance({command.header.issued_at, static_cast<std::uint32_t>(distance)});
            if (progress.reached_second != command.header.issued_at) return LoadError::InvalidTime;
        }
        if (candidate.submit(command) != SubmitResult{SubmitError::None, s.receipts[i]})
            return LoadError::InvalidValue;
    }
    if (s.second > candidate.second_) {
        const auto distance = s.second - candidate.second_;
        if (distance > static_cast<SimSecond>(s.limits.max_events) * 5 + 5)
            return LoadError::InvalidTime;
        const auto progress = candidate.advance({s.second, static_cast<std::uint32_t>(distance)});
        if (progress.reached_second != s.second) return LoadError::InvalidTime;
    }
    if (candidate.second_ != s.second || candidate.decision_index_ != s.decision_index ||
        candidate.decision_pending_ != s.decision_pending || candidate.revision_ != s.revision ||
        candidate.climate_boundaries_ != s.climate_boundaries ||
        candidate.crop_boundaries_ != s.crop_boundaries || candidate.controls_ != s.controls ||
        candidate.commands_ != s.commands || candidate.receipts_ != s.receipts ||
        candidate.events_ != s.events) return LoadError::InvalidValue;
    destination.swap(candidate);
    return LoadError::None;
}
} // namespace canopy
