#pragma once

#include <canopy/id.hpp>
#include <canopy/units.hpp>

#include <cstdint>
#include <variant>

namespace canopy {
using SimSecond = std::int64_t;

struct CommandHeader {
    Id command_id;
    Id actor_id;
    std::uint64_t expected_revision;
    SimSecond issued_at;
    friend bool operator==(const CommandHeader&, const CommandHeader&) = default;
};
struct SetControl {
    Id entity_id;
    Fraction requested;
    friend bool operator==(const SetControl&, const SetControl&) = default;
};
struct ResolveDecision {
    Id decision_id;
    SimSecond decision_second;
    friend bool operator==(const ResolveDecision&, const ResolveDecision&) = default;
};
struct Command {
    CommandHeader header;
    std::variant<SetControl, ResolveDecision> action;
    friend bool operator==(const Command&, const Command&) = default;
};

enum class SubmitError {
    None, InvalidId, InvalidTime, InvalidValue, UnknownEntity,
    InvalidDecision, StaleRevision, Conflict, CapacityExceeded, InvalidModel
};
struct Receipt {
    Id command_id;
    std::uint64_t revision;
    SimSecond accepted_at;
    friend bool operator==(const Receipt&, const Receipt&) = default;
};
struct SubmitResult {
    SubmitError error{};
    Receipt receipt{};
    friend bool operator==(const SubmitResult&, const SubmitResult&) = default;
};
} // namespace canopy
