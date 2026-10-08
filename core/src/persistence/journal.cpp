#include "internal.hpp"
#include "command-log_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <algorithm>

namespace canopy::persistence {
namespace d = canopy::disk;
namespace {
auto put_id(flatbuffers::FlatBufferBuilder& b, Id id) { return d::CreateId(b, id.high, id.low); }
Id get_id(const d::Id* id) { return id ? Id{id->high(), id->low()} : Id{}; }
}
std::vector<std::uint8_t> encode_journal(std::span<const JournalEntry> entries, std::uint64_t revision,
                                         SimSecond second, const JournalProvenance& provenance) {
    flatbuffers::FlatBufferBuilder b;
    std::vector<flatbuffers::Offset<d::Transition>> transitions;
    for (const auto& entry : entries) {
        flatbuffers::Offset<d::Command> command;
        flatbuffers::Offset<d::Receipt> receipt;
        if (entry.kind == JournalEntry::Kind::Submit) {
            const auto& c = entry.command;
            const bool control = std::holds_alternative<SetControl>(c.action);
            const Id entity = control ? std::get<SetControl>(c.action).entity_id : std::get<ResolveDecision>(c.action).decision_id;
            command = d::CreateCommand(b, put_id(b, c.header.command_id), put_id(b, c.header.actor_id),
                c.header.expected_revision, c.header.issued_at, static_cast<std::uint8_t>(control ? 1 : 2),
                put_id(b, entity), control ? std::get<SetControl>(c.action).requested.value : 0,
                control ? 0 : std::get<ResolveDecision>(c.action).decision_second);
            receipt = d::CreateReceipt(b, put_id(b, entry.outcome.receipt.command_id),
                entry.outcome.receipt.revision, entry.outcome.receipt.accepted_at);
        }
        transitions.push_back(d::CreateTransition(b, static_cast<std::uint8_t>(entry.kind), command,
            static_cast<std::uint8_t>(entry.outcome.error), receipt, entry.budget.target_second,
            entry.budget.max_substeps, entry.progress.reached_second, static_cast<std::uint8_t>(entry.progress.stop)));
    }
    b.Finish(d::CreateJournal(b, 2, revision, second, b.CreateVector(transitions),
        put_id(b, provenance.campaign), put_id(b, provenance.branch),
        b.CreateVector(provenance.checkpoint.data(), provenance.checkpoint.size()),
        b.CreateVector(provenance.predecessor.data(), provenance.predecessor.size())), d::JournalIdentifier());
    return {b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize()};
}
SaveError decode_journal(std::span<const std::uint8_t> bytes, std::vector<JournalEntry>& entries,
                         std::uint64_t& revision, SimSecond& second, JournalProvenance& provenance) {
    if (bytes.size() > 16U * 1024U * 1024U || bytes.size() < 8) return SaveError::Corrupt;
    flatbuffers::Verifier verifier(bytes.data(), bytes.size(), 64, 400000);
    if (!d::VerifyJournalBuffer(verifier)) return SaveError::Corrupt;
    const auto* journal = d::GetJournal(bytes.data());
    if (journal->version() != 2) return SaveError::UnsupportedVersion;
    if (!journal->entries() || journal->entries()->size() > 65536) return SaveError::Corrupt;
    if (entries.size() > 1000000 - journal->entries()->size()) return SaveError::CapacityExceeded;
    revision = journal->start_revision();
    second = journal->start_second();
    if (!journal->campaign() || !journal->branch() || !journal->checkpoint_sha256() ||
        !journal->predecessor_sha256() || journal->checkpoint_sha256()->size() != 32 ||
        journal->predecessor_sha256()->size() != 32) return SaveError::Corrupt;
    provenance.campaign = get_id(journal->campaign());
    provenance.branch = get_id(journal->branch());
    std::copy_n(journal->checkpoint_sha256()->data(), 32, provenance.checkpoint.begin());
    std::copy_n(journal->predecessor_sha256()->data(), 32, provenance.predecessor.begin());
    for (const auto* e : *journal->entries()) {
        if (e->kind() == 0) {
            const auto* c = e->command(); const auto* r = e->receipt();
            if (!c || !r || !c->id() || !c->actor() || !c->entity() || !r->id() ||
                (c->kind() != 1 && c->kind() != 2) || e->error() > static_cast<unsigned>(SubmitError::InvalidModel))
                return SaveError::Corrupt;
            Command command{{get_id(c->id()), get_id(c->actor()), c->expected(), c->issued()},
                c->kind() == 1 ? std::variant<SetControl, ResolveDecision>{SetControl{get_id(c->entity()), Fraction{c->value()}}}
                               : std::variant<SetControl, ResolveDecision>{ResolveDecision{get_id(c->entity()), c->decision_second()}}};
            entries.push_back(JournalEntry::submission(command, {static_cast<SubmitError>(e->error()),
                {get_id(r->id()), r->revision(), r->accepted()}}));
        } else if (e->kind() == 1) {
            if (e->stop() > static_cast<unsigned>(AdvanceStop::CapacityExhausted) || e->command() || e->receipt())
                return SaveError::Corrupt;
            entries.push_back(JournalEntry::advance({e->target(), e->max_substeps()},
                {e->reached(), static_cast<AdvanceStop>(e->stop())}));
        } else return SaveError::Corrupt;
    }
    return SaveError::None;
}
SaveError replay_journal(std::span<const std::uint8_t> bytes, World& world,
                         const JournalProvenance& expected) {
    std::vector<JournalEntry> entries;
    std::uint64_t revision{};
    SimSecond second{};
    JournalProvenance provenance;
    auto error = decode_journal(bytes, entries, revision, second, provenance);
    if (error != SaveError::None) return error;
    if (revision != world.view().revision || second != world.view().second ||
        provenance.campaign != expected.campaign || provenance.branch != expected.branch ||
        provenance.checkpoint != expected.checkpoint ||
        provenance.predecessor != expected.predecessor) return SaveError::Corrupt;
    for (const auto& entry : entries) {
        if (entry.kind == JournalEntry::Kind::Submit) {
            if (world.submit(entry.command) != entry.outcome) return SaveError::Corrupt;
        } else if (world.advance(entry.budget) != entry.progress) return SaveError::Corrupt;
    }
    return SaveError::None;
}
} // namespace canopy::persistence
