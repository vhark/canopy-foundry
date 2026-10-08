#include <canopy/save.hpp>
#include "save_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <cmath>
#include <limits>

namespace canopy::persistence {
namespace d = canopy::disk;
namespace {
flatbuffers::Offset<d::Id> put_id(flatbuffers::FlatBufferBuilder& b, Id id) {
    return d::CreateId(b, id.high, id.low);
}
Id get_id(const d::Id* id) { return id ? Id{id->high(), id->low()} : Id{}; }
}
std::vector<std::uint8_t> encode(const WorldState& s, const SaveIdentity& identity) {
    flatbuffers::FlatBufferBuilder b;
    std::vector<flatbuffers::Offset<d::Initial>> initials;
    std::vector<flatbuffers::Offset<d::Deadline>> deadlines;
    std::vector<flatbuffers::Offset<d::Control>> controls;
    std::vector<flatbuffers::Offset<d::Command>> commands;
    std::vector<flatbuffers::Offset<d::Receipt>> receipts;
    std::vector<flatbuffers::Offset<d::Event>> events;
    for (const auto& c : s.initial_controls) initials.push_back(d::CreateInitial(b, put_id(b, c.id), c.initial.value));
    for (const auto& v : s.deadlines) deadlines.push_back(d::CreateDeadline(b, put_id(b, v.id), put_id(b, v.entity_id), v.second));
    for (const auto& v : s.controls) controls.push_back(d::CreateControl(b, put_id(b, v.id), v.requested.value, v.applied.value));
    for (const auto& c : s.commands) {
        const bool control = std::holds_alternative<SetControl>(c.action);
        const Id entity = control ? std::get<SetControl>(c.action).entity_id : std::get<ResolveDecision>(c.action).decision_id;
        const double value = control ? std::get<SetControl>(c.action).requested.value : 0.0;
        const SimSecond second = control ? 0 : std::get<ResolveDecision>(c.action).decision_second;
        commands.push_back(d::CreateCommand(b, put_id(b, c.header.command_id), put_id(b, c.header.actor_id),
            c.header.expected_revision, c.header.issued_at, static_cast<std::uint8_t>(control ? 1 : 2),
            put_id(b, entity), value, second));
    }
    for (const auto& r : s.receipts) receipts.push_back(d::CreateReceipt(b, put_id(b, r.command_id), r.revision, r.accepted_at));
    for (const auto& e : s.events) events.push_back(d::CreateEvent(b, e.second, static_cast<std::uint8_t>(e.type),
        put_id(b, e.entity), put_id(b, e.command_id), e.value.value));
    const auto state = d::CreateState(b, static_cast<std::uint32_t>(s.limits.max_entities),
        static_cast<std::uint32_t>(s.limits.max_receipts), static_cast<std::uint32_t>(s.limits.max_events),
        static_cast<std::uint32_t>(s.limits.max_decisions), s.second,
        static_cast<std::uint32_t>(s.decision_index), s.decision_pending, s.revision, s.seed,
        s.climate_boundaries, s.crop_boundaries, b.CreateVector(initials), b.CreateVector(deadlines),
        b.CreateVector(controls), b.CreateVector(commands), b.CreateVector(receipts), b.CreateVector(events));
    const auto tag = d::CreateIdentity(b, put_id(b, identity.campaign), put_id(b, identity.branch),
        put_id(b, identity.source_campaign), b.CreateString(identity.source), b.CreateString(identity.model),
        b.CreateString(identity.render), put_id(b, identity.parent_branch));
    b.Finish(d::CreateSave(b, 2, tag, state), d::SaveIdentifier());
    return {b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize()};
}
SaveError decode(std::span<const std::uint8_t> bytes, WorldState& s, SaveIdentity& identity) {
    if (bytes.size() > 64U * 1024U * 1024U || bytes.size() < 8) return SaveError::Corrupt;
    flatbuffers::Verifier verifier(bytes.data(), bytes.size(), 64, 1200000);
    if (!d::VerifySaveBuffer(verifier)) return SaveError::Corrupt;
    const auto* save = d::GetSave(bytes.data());
    if (save->version() != 2) return SaveError::UnsupportedVersion;
    if (!save->state() || !save->identity()) return SaveError::Corrupt;
    const auto* tag = save->identity();
    if (!tag->campaign() || !tag->branch() || !tag->source_campaign() ||
        !tag->source() || !tag->model() || !tag->render() ||
        tag->source()->size() > 128 || tag->model()->size() > 128 || tag->render()->size() > 128)
        return SaveError::Corrupt;
    identity = {get_id(tag->campaign()), get_id(tag->branch()), get_id(tag->source_campaign()),
                tag->source()->str(), tag->model()->str(), tag->render()->str(), get_id(tag->parent_branch())};
    if (identity.campaign == invalid_id || identity.branch == invalid_id || identity.model.empty()) return SaveError::Corrupt;
    const auto* state = save->state();
    s.limits = {state->max_entities(), state->max_receipts(), state->max_events(), state->max_decisions()};
    s.second = state->second(); s.decision_index = state->decision_index();
    s.decision_pending = state->pending(); s.revision = state->revision(); s.seed = state->seed();
    s.climate_boundaries = state->climate(); s.crop_boundaries = state->crop();
    if (!state->initial() || !state->deadlines() || !state->controls() ||
        !state->commands() || !state->receipts() || !state->events() ||
        s.limits.max_entities > 4096 || s.limits.max_receipts > 65536 ||
        s.limits.max_events > 262144 || s.limits.max_decisions > 4096 ||
        state->initial()->size() > s.limits.max_entities || state->deadlines()->size() > s.limits.max_decisions ||
        state->controls()->size() > s.limits.max_entities || state->commands()->size() > s.limits.max_receipts ||
        state->receipts()->size() > s.limits.max_receipts || state->events()->size() > s.limits.max_events)
        return SaveError::CapacityExceeded;
    for (const auto* v : *state->initial()) { if (!v->id()) return SaveError::Corrupt; s.initial_controls.push_back({get_id(v->id()), Fraction{v->value()}}); }
    for (const auto* v : *state->deadlines()) { if (!v->id() || !v->entity()) return SaveError::Corrupt; s.deadlines.push_back({get_id(v->id()), get_id(v->entity()), v->second()}); }
    for (const auto* v : *state->controls()) { if (!v->id()) return SaveError::Corrupt; s.controls.push_back({get_id(v->id()), Fraction{v->requested()}, Fraction{v->applied()}}); }
    for (const auto* v : *state->commands()) {
        if (!v->id() || !v->actor() || !v->entity() || (v->kind() != 1 && v->kind() != 2)) return SaveError::Corrupt;
        Command c{{get_id(v->id()), get_id(v->actor()), v->expected(), v->issued()},
            v->kind() == 1 ? std::variant<SetControl, ResolveDecision>{SetControl{get_id(v->entity()), Fraction{v->value()}}}
                           : std::variant<SetControl, ResolveDecision>{ResolveDecision{get_id(v->entity()), v->decision_second()}}};
        s.commands.push_back(c);
    }
    for (const auto* v : *state->receipts()) { if (!v->id()) return SaveError::Corrupt; s.receipts.push_back({get_id(v->id()), v->revision(), v->accepted()}); }
    for (const auto* v : *state->events()) { if (!v->entity() || !v->command() || v->kind() > 5) return SaveError::Corrupt; s.events.push_back({v->second(), static_cast<EventType>(v->kind()), get_id(v->entity()), get_id(v->command()), Fraction{v->value()}}); }
    return SaveError::None;
}
} // namespace canopy::persistence
