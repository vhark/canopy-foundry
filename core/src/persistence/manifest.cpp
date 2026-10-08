#include "internal.hpp"
#include "command-log_generated.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <sstream>
#include <set>
#include <string_view>
#include <tuple>

namespace canopy {
namespace {
struct Manifest {
    std::uint64_t generation{}, revision{};
    SimSecond second{};
    CheckpointKind kind{};
    std::string snapshot;
    std::vector<std::string> journals;
    std::vector<std::string> audit;
};
bool safe_name(const std::string& name, char prefix) {
    if (name.size() < 3 || name.size() > 32 || name[0] != prefix || name[1] != '-') return false;
    return std::all_of(name.begin() + 2, name.end(), [](char c) { return c >= '0' && c <= '9'; });
}
std::string manifest_bytes(const Manifest& m) {
    std::ostringstream out;
    out << "CFM2 " << m.generation << ' ' << m.revision << ' ' << m.second << ' '
        << static_cast<int>(m.kind) << ' ' << m.snapshot << ' ' << m.journals.size()
        << ' ' << m.audit.size() << '\n';
    for (const auto& j : m.journals) out << j << '\n';
    for (const auto& j : m.audit) out << j << '\n';
    return out.str();
}
SaveError parse_text(const std::string& text, Manifest& m) {
    m = {};
    std::istringstream in(text);
    std::string magic; int kind; std::size_t count, audit_count;
    if (!(in >> magic >> m.generation >> m.revision >> m.second >> kind >> m.snapshot >> count >> audit_count) ||
        magic != "CFM2" || m.generation == 0 || m.second < 0 || kind < 0 || kind > 2 ||
        count > 65536 || audit_count > 65536 || count + audit_count > 65536 ||
        !safe_name(m.snapshot, 's')) return SaveError::Corrupt;
    m.kind = static_cast<CheckpointKind>(kind);
    for (std::size_t i = 0; i < count; ++i) {
        std::string name;
        if (!(in >> name) || !safe_name(name, 'j')) return SaveError::Corrupt;
        m.journals.push_back(name);
    }
    std::string excess;
    for (std::size_t i = 0; i < audit_count; ++i) {
        std::string name;
        if (!(in >> name) || !safe_name(name, 'j')) return SaveError::Corrupt;
        m.audit.push_back(name);
    }
    if (in >> excess) return SaveError::Corrupt;
    return SaveError::None;
}
SaveError parse(const std::filesystem::path& path, Manifest& m) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return SaveError::NotFound;
    if (size > 4U * 1024U * 1024U || size < 16) return SaveError::Corrupt;
    std::ifstream in(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_text(text, m);
}
std::uint64_t used_bytes(const std::filesystem::path& root) {
    std::error_code ec; std::uint64_t total = 0;
    for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            const auto n = it->file_size(ec);
            if (ec || n > UINT64_MAX - total) return UINT64_MAX;
            total += n;
        }
    }
    return ec ? UINT64_MAX : total;
}
SaveError prepare(const std::filesystem::path& root, std::uint64_t budget) {
    if (budget == 0) return SaveError::BudgetExceeded;
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return SaveError::Io;
    return used_bytes(root) >= budget ? SaveError::BudgetExceeded : SaveError::None;
}
std::optional<World> audit_origin(const std::filesystem::path& root,
                                  const persistence::JournalProvenance& provenance,
                                  SaveIdentity& origin_identity) {
    std::error_code ec;
    for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!safe_name(it->path().filename().string(), 's')) continue;
        std::vector<std::uint8_t> bytes;
        if (persistence::read_artifact(it->path(), bytes, persistence::max_snapshot_bytes) != SaveError::None ||
            persistence::sha256(bytes) != provenance.checkpoint) continue;
        WorldState state;
        SaveIdentity identity;
        if (persistence::decode(bytes, state, identity) != SaveError::None ||
            identity.campaign != provenance.campaign || identity.branch != provenance.branch) return {};
        World world(state.limits);
        if (World::restore(state, world) != LoadError::None) return {};
        origin_identity = std::move(identity);
        return std::optional<World>{std::move(world)};
    }
    return {};
}
LoadResult load_manifest(const std::filesystem::path& root, const Manifest& m,
                         const std::string& model, const std::string& render) {
    LoadResult result;
    std::vector<std::uint8_t> bytes;
    result.error = persistence::read_artifact(root / m.snapshot, bytes, persistence::max_snapshot_bytes);
    if (result.error != SaveError::None) return result;
    WorldState state;
    result.error = persistence::decode(bytes, state, result.identity);
    if (result.error != SaveError::None) return result;
    if (result.identity.model != model) { result.error = SaveError::MissingModel; return result; }
    result.neutral_render = result.identity.render != render;
    World restored(state.limits);
    if (World::restore(state, restored) != LoadError::None) { result.error = SaveError::Corrupt; return result; }
    const auto checkpoint_digest = persistence::sha256(bytes);
    persistence::JournalProvenance chain{result.identity.campaign, result.identity.branch,
                                         checkpoint_digest, checkpoint_digest};
    for (const auto& name : m.journals) {
        bytes.clear();
        result.error = persistence::read_artifact(root / name, bytes, persistence::max_journal_bytes);
        if (result.error != SaveError::None) return result;
        result.error = persistence::replay_journal(bytes, restored, chain);
        if (result.error != SaveError::None) return result;
        chain.predecessor = persistence::sha256(bytes);
    }
    std::optional<World> audited;
    persistence::JournalProvenance audit_chain{};
    SaveIdentity origin_identity;
    for (const auto& name : m.audit) {
        bytes.clear();
        result.error = persistence::read_artifact(root / name, bytes, persistence::max_journal_bytes);
        if (result.error != SaveError::None) return result;
        std::vector<JournalEntry> outcomes;
        std::uint64_t revision{};
        SimSecond second{};
        persistence::JournalProvenance provenance;
        result.error = persistence::decode_journal(bytes, outcomes, revision, second, provenance);
        if (result.error != SaveError::None) return result;
        if (provenance.predecessor == provenance.checkpoint) {
            audited = audit_origin(root, provenance, origin_identity);
            if (!audited || origin_identity.campaign != result.identity.campaign ||
                origin_identity.source_campaign != result.identity.source_campaign ||
                origin_identity.source != result.identity.source ||
                origin_identity.model != result.identity.model ||
                (origin_identity.branch != result.identity.branch &&
                 result.identity.parent_branch == Id{})) {
                result.error = SaveError::Corrupt;
                return result;
            }
            audit_chain = {origin_identity.campaign, origin_identity.branch,
                           provenance.checkpoint, provenance.checkpoint};
        }
        if (!audited) { result.error = SaveError::Corrupt; return result; }
        result.error = persistence::replay_journal(bytes, *audited, audit_chain);
        if (result.error != SaveError::None) return result;
        audit_chain.predecessor = persistence::sha256(bytes);
        const auto audited_state = audited->capture();
        if (audited_state.commands.size() > state.commands.size() ||
            audited_state.events.size() > state.events.size()) {
            result.error = SaveError::Corrupt; return result;
        }
        for (std::size_t i = 0; i < audited_state.commands.size(); ++i)
            if (audited_state.commands[i] != state.commands[i] ||
                audited_state.receipts[i] != state.receipts[i]) {
                result.error = SaveError::Corrupt; return result;
            }
        std::size_t cursor = 0;
        for (const auto& event : audited_state.events) {
            while (cursor < state.events.size() &&
                   std::tie(state.events[cursor].second, state.events[cursor].type,
                            state.events[cursor].entity, state.events[cursor].command_id) <
                   std::tie(event.second, event.type, event.entity, event.command_id))
                ++cursor;
            if (cursor == state.events.size() || state.events[cursor] != event) {
                result.error = SaveError::Corrupt; return result;
            }
            ++cursor;
        }
    }
    if (restored.view().revision != m.revision || restored.view().second != m.second) {
        result.error = SaveError::Corrupt; return result;
    }
    result.world.emplace(std::move(restored));
    return result;
}
LoadResult load_any(const std::filesystem::path& root, const std::string& model, const std::string& render,
                    Manifest* actual = nullptr) {
    Manifest m;
    const auto status = parse(root / "manifest", m);
    auto current = status == SaveError::None ? load_manifest(root, m, model, render) : LoadResult{status};
    if (current.error == SaveError::None) { if (actual) *actual = m; return current; }
    if (current.error == SaveError::MissingModel || current.error == SaveError::UnsupportedVersion) return current;
    Manifest previous;
    if (parse(root / "manifest.previous", previous) != SaveError::None) return current;
    auto old = load_manifest(root, previous, model, render);
    if (old.error == SaveError::None) {
        old.recovered_previous = true;
        if (actual) *actual = previous;
        return old;
    }
    return current;
}
std::string name(char kind, std::uint64_t sequence) { return std::string(1, kind) + '-' + std::to_string(sequence); }
SaveError history_manifest(const std::filesystem::path& root, std::uint64_t generation, Manifest& m) {
    std::vector<std::uint8_t> bytes;
    auto error = persistence::read_artifact(root / name('h', generation), bytes, 4U * 1024U * 1024U);
    if (error != SaveError::None) return error;
    return parse_text(std::string(bytes.begin(), bytes.end()), m);
}
SaveError verify_manifest(const std::filesystem::path& root, const Manifest& m) {
    std::vector<std::uint8_t> bytes;
    WorldState state;
    SaveIdentity identity;
    auto error = persistence::read_artifact(root / m.snapshot, bytes, persistence::max_snapshot_bytes);
    if (error != SaveError::None) return error;
    error = persistence::decode(bytes, state, identity);
    if (error != SaveError::None) return error;
    return load_manifest(root, m, identity.model, identity.render).error;
}
std::vector<Manifest> archives(const std::filesystem::path& root, std::uint64_t committed) {
    std::vector<Manifest> result;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto filename = it->path().filename().string();
        if (!safe_name(filename, 'h')) continue;
        std::uint64_t generation{};
        const auto digits = std::string_view(filename).substr(2);
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), generation);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || generation > committed) continue;
        Manifest m;
        if (history_manifest(root, generation, m) != SaveError::None || m.generation != generation) continue;
        if (verify_manifest(root, m) != SaveError::None) continue;
        result.push_back(std::move(m));
    }
    if (ec) result.clear();
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.generation < b.generation; });
    return result;
}
SaveError journal_provenance(const std::filesystem::path& root, const std::string& filename,
                             persistence::JournalProvenance& provenance) {
    std::vector<std::uint8_t> bytes;
    auto error = persistence::read_artifact(root / filename, bytes, persistence::max_journal_bytes);
    if (error != SaveError::None) return error;
    std::vector<JournalEntry> entries;
    std::uint64_t revision{};
    SimSecond second{};
    return persistence::decode_journal(bytes, entries, revision, second, provenance);
}
SaveError snapshot_digest(const std::filesystem::path& root, const std::string& filename,
                          persistence::Digest& digest) {
    std::vector<std::uint8_t> bytes;
    const auto error = persistence::read_artifact(root / filename, bytes, persistence::max_snapshot_bytes);
    if (error != SaveError::None) return error;
    digest = persistence::sha256(bytes);
    return SaveError::None;
}
SaveError origin_snapshot(const std::filesystem::path& root, const std::string& journal,
                          const persistence::Digest& checkpoint, std::string& origin) {
    const auto entry_digits = std::string_view(journal).substr(2);
    std::uint64_t entry_generation{};
    const auto entry_parsed = std::from_chars(entry_digits.data(),
                                               entry_digits.data() + entry_digits.size(), entry_generation);
    if (entry_parsed.ec != std::errc{} ||
        entry_parsed.ptr != entry_digits.data() + entry_digits.size()) return SaveError::Corrupt;
    std::uint64_t latest_generation{};
    std::error_code ec;
    for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto filename = it->path().filename().string();
        if (!safe_name(filename, 's')) continue;
        const auto digits = std::string_view(filename).substr(2);
        std::uint64_t generation{};
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), generation);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
            generation >= entry_generation || generation <= latest_generation) continue;
        persistence::Digest digest;
        if (snapshot_digest(root, filename, digest) != SaveError::None || digest != checkpoint) continue;
        origin = filename;
        latest_generation = generation;
    }
    return ec ? SaveError::Io : origin.empty() ? SaveError::Corrupt : SaveError::None;
}
SaveError compact_audit(const std::filesystem::path& root, Manifest& next,
                        std::uint64_t prior_generation) {
    if (next.audit.empty()) return SaveError::None;
    const auto available = archives(root, prior_generation);
    std::set<std::string> retained_origins;
    std::size_t daily = 0;
    const std::size_t keep_prior_daily = next.kind == CheckpointKind::Daily ? 6 : 7;
    for (auto it = available.rbegin(); it != available.rend(); ++it) {
        if (it->kind == CheckpointKind::Daily && daily++ >= keep_prior_daily) continue;
        retained_origins.insert(it->snapshot);
    }
    std::vector<std::string> retained;
    std::string origin;
    for (const auto& journal : next.audit) {
        persistence::JournalProvenance provenance;
        const auto error = journal_provenance(root, journal, provenance);
        if (error != SaveError::None) return error;
        if (provenance.predecessor == provenance.checkpoint) {
            origin.clear();
            const auto origin_error = origin_snapshot(root, journal, provenance.checkpoint, origin);
            if (origin_error != SaveError::None) return origin_error;
        }
        if (retained_origins.contains(origin)) retained.push_back(journal);
    }
    next.audit = std::move(retained);
    return SaveError::None;
}
SaveError prune_daily(const std::filesystem::path& root, std::uint64_t committed) {
    auto available = archives(root, committed);
    std::set<std::uint64_t> keep_generations;
    std::vector<Manifest> retained;
    std::size_t daily = 0;
    for (auto it = available.rbegin(); it != available.rend(); ++it) {
        if (it->kind != CheckpointKind::Daily || daily++ < 7) {
            retained.push_back(*it);
            keep_generations.insert(it->generation);
        }
    }
    Manifest current;
    if (parse(root / "manifest", current) != SaveError::None) return SaveError::Corrupt;
    retained.push_back(current);
    Manifest previous;
    if (parse(root / "manifest.previous", previous) == SaveError::None) retained.push_back(previous);
    std::set<std::string> protected_names;
    std::set<std::string> protected_origins;
    for (const auto& entry : retained) {
        protected_names.insert(entry.snapshot);
        for (const auto& journal : entry.journals) protected_names.insert(journal);
        std::string origin;
        for (const auto& journal : entry.audit) {
            protected_names.insert(journal);
            persistence::JournalProvenance provenance;
            const auto error = journal_provenance(root, journal, provenance);
            if (error != SaveError::None) return error;
            if (provenance.predecessor == provenance.checkpoint) {
                origin.clear();
                const auto origin_error = origin_snapshot(root, journal, provenance.checkpoint, origin);
                if (origin_error != SaveError::None) return origin_error;
                protected_origins.insert(origin);
            }
        }
    }
    std::error_code ec;
    for (const auto& entry : available) {
        if (keep_generations.contains(entry.generation)) continue;
        std::filesystem::remove(root / name('h', entry.generation), ec);
        if (ec) return SaveError::Io;
    }
    std::vector<std::filesystem::path> expired;
    for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto filename = it->path().filename().string();
        if (protected_names.contains(filename)) continue;
        if (safe_name(filename, 's')) {
            if (protected_origins.contains(filename)) continue;
            expired.push_back(it->path());
        } else if (safe_name(filename, 'j')) {
            expired.push_back(it->path());
        }
    }
    if (ec) return SaveError::Io;
    for (const auto& filename : expired) {
        std::filesystem::remove(filename, ec);
        if (ec) return SaveError::Io;
    }
    return SaveError::None;
}
SaveStatus publish(const std::filesystem::path& root, const Manifest& m, std::uint64_t budget,
                   bool preserve_previous = false) {
    const auto text = manifest_bytes(m);
    if (used_bytes(root) + text.size() * 3 + 1024 > budget) return {SaveError::BudgetExceeded};
    const auto bytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    if (m.journals.empty()) {
        const auto archived = persistence::write_artifact(root / name('h', m.generation), bytes, 4U * 1024U * 1024U);
        if (archived != SaveError::None) return {archived};
    }
    return {persistence::atomic_manifest(root, bytes, preserve_previous)};
}
OutcomesResult outcomes_for(const std::filesystem::path& root, const Manifest& m) {
    OutcomesResult result;
    result.first_generation = m.generation;
    result.last_generation = m.generation;
    bool origin_set = false;
    auto append = [&](const std::string& entry_name) {
        std::vector<std::uint8_t> bytes;
        auto error = persistence::read_artifact(root / entry_name, bytes, persistence::max_journal_bytes);
        if (error != SaveError::None) return error;
        std::uint64_t revision{};
        SimSecond second{};
        persistence::JournalProvenance provenance;
        error = persistence::decode_journal(bytes, result.entries, revision, second, provenance);
        if (error != SaveError::None) return error;
        if (!origin_set && !result.entries.empty()) {
            std::string origin;
            error = origin_snapshot(root, entry_name, provenance.checkpoint, origin);
            if (error != SaveError::None) return error;
            const auto digits = std::string_view(origin).substr(2);
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(),
                                                result.first_generation);
            if (parsed.ec != std::errc{}) return SaveError::Corrupt;
            origin_set = true;
        }
        return SaveError::None;
    };
    for (const auto& entry_name : m.audit) {
        result.error = append(entry_name);
        if (result.error != SaveError::None) return result;
    }
    for (const auto& entry_name : m.journals) {
        result.error = append(entry_name);
        if (result.error != SaveError::None) return result;
    }
    return result;
}
} // namespace

SaveStatus save_checkpoint(const std::filesystem::path& root, WorldState state,
                           const SaveIdentity& identity, CheckpointKind kind, std::uint64_t budget) {
    if (identity.campaign == invalid_id || identity.branch == invalid_id || identity.model.empty() ||
        identity.parent_branch == identity.branch || identity.source.size() > 128 ||
        identity.model.size() > 128 || identity.render.size() > 128)
        return {SaveError::InvalidState};
    auto status = prepare(root, budget);
    if (status != SaveError::None) return {status};
    Manifest prior;
    auto previous = load_any(root, identity.model, identity.render, &prior);
    if (previous.error != SaveError::None && previous.error != SaveError::NotFound) return {previous.error};
    if (previous.error == SaveError::None &&
        (previous.identity.campaign != identity.campaign || previous.identity.branch != identity.branch ||
         previous.identity.source != identity.source || previous.identity.model != identity.model ||
         previous.identity.render != identity.render ||
         previous.identity.parent_branch != identity.parent_branch ||
         previous.identity.source_campaign != identity.source_campaign)) return {SaveError::Incompatible};
    World checked(state.limits);
    if (World::restore(state, checked) != LoadError::None) return {SaveError::InvalidState};
    auto bytes = persistence::encode(state, identity);
    if (bytes.size() > persistence::max_snapshot_bytes) return {SaveError::CapacityExceeded};
    Manifest newest;
    const auto seen = parse(root / "manifest", newest);
    std::uint64_t generation = previous.error == SaveError::None ? prior.generation : 0;
    if (seen == SaveError::None) generation = std::max(generation, newest.generation);
    if (generation == UINT64_MAX) return {SaveError::CapacityExceeded};
    ++generation;
    while (std::filesystem::exists(root / name('s', generation)) ||
           std::filesystem::exists(root / name('h', generation))) {
        if (generation == UINT64_MAX) return {SaveError::CapacityExceeded};
        ++generation;
    }
    const auto snapshot = name('s', generation);
    if (used_bytes(root) + bytes.size() + 65536 > budget) return {SaveError::BudgetExceeded};
    Manifest next{generation, state.revision, state.second, kind, snapshot, {}, {}};
    if (previous.error == SaveError::None) {
        next.audit = prior.audit;
        next.audit.insert(next.audit.end(), prior.journals.begin(), prior.journals.end());
        status = compact_audit(root, next, prior.generation);
        if (status != SaveError::None) return {status};
        if (next.audit.size() > 65536) return {SaveError::CapacityExceeded};
    }
    status = persistence::write_artifact(root / snapshot, bytes, persistence::max_snapshot_bytes);
    if (status != SaveError::None) return {status};
    const auto committed = publish(root, next, budget, previous.recovered_previous);
    if (committed.error != SaveError::None) return committed;
    return {prune_daily(root, generation)};
}
SaveStatus save_checkpoint(const std::filesystem::path& root, const World& world,
                           const SaveIdentity& identity, CheckpointKind kind, std::uint64_t budget) {
    return save_checkpoint(root, world.capture(), identity, kind, budget);
}
SaveStatus append_journal(const std::filesystem::path& root, std::span<const JournalEntry> entries,
                          std::uint64_t budget) {
    if (entries.empty() || entries.size() > 65536) return {SaveError::InvalidState};
    Manifest m;
    Manifest candidate;
    SaveError error = parse(root / "manifest", candidate);
    if (error != SaveError::None) error = parse(root / "manifest.previous", candidate);
    if (error != SaveError::None) return {error};
    std::vector<std::uint8_t> bytes;
    WorldState state;
    SaveIdentity identity;
    auto decode_candidate = [&] {
        bytes.clear();
        auto candidate_error = persistence::read_artifact(root / candidate.snapshot, bytes,
                                                           persistence::max_snapshot_bytes);
        if (candidate_error == SaveError::None)
            candidate_error = persistence::decode(bytes, state, identity);
        return candidate_error;
    };
    error = decode_candidate();
    if (error == SaveError::UnsupportedVersion) return {error};
    if (error != SaveError::None) {
        error = parse(root / "manifest.previous", candidate);
        if (error != SaveError::None) return {error};
        error = decode_candidate();
        if (error != SaveError::None) return {error};
    }
    auto loaded = load_any(root, identity.model, identity.render, &m);
    if (loaded.error != SaveError::None) return {loaded.error};
    if (m.journals.size() + m.audit.size() >= 65536 || m.generation == UINT64_MAX)
        return {SaveError::CapacityExceeded};
    const auto starting_revision = loaded.world->view().revision;
    const auto starting_second = loaded.world->view().second;
    bytes.clear();
    error = persistence::read_artifact(root / m.snapshot, bytes, persistence::max_snapshot_bytes);
    if (error != SaveError::None) return {error};
    const auto checkpoint_digest = persistence::sha256(bytes);
    persistence::JournalProvenance chain{loaded.identity.campaign, loaded.identity.branch,
                                         checkpoint_digest, checkpoint_digest};
    if (!m.journals.empty()) {
        bytes.clear();
        error = persistence::read_artifact(root / m.journals.back(), bytes, persistence::max_journal_bytes);
        if (error != SaveError::None) return {error};
        chain.predecessor = persistence::sha256(bytes);
    }
    bytes = persistence::encode_journal(entries, starting_revision, starting_second, chain);
    if (bytes.size() > persistence::max_journal_bytes) return {SaveError::CapacityExceeded};
    error = persistence::replay_journal(bytes, *loaded.world, chain);
    if (error != SaveError::None) return {error};
    Manifest newest;
    const auto latest = parse(root / "manifest", newest);
    std::uint64_t generation = m.generation;
    if (latest == SaveError::None) generation = std::max(generation, newest.generation);
    if (generation == UINT64_MAX) return {SaveError::CapacityExceeded};
    do {
        ++generation;
        if (generation == UINT64_MAX) return {SaveError::CapacityExceeded};
    } while (std::filesystem::exists(root / name('j', generation)) ||
             std::filesystem::exists(root / name('s', generation)) ||
             std::filesystem::exists(root / name('h', generation)));
    m.journals.push_back(name('j', generation));
    m.generation = generation;
    m.revision = loaded.world->view().revision;
    m.second = loaded.world->view().second;
    if (used_bytes(root) + bytes.size() + 65536 > budget) return {SaveError::BudgetExceeded};
    error = persistence::write_artifact(root / m.journals.back(), bytes, persistence::max_journal_bytes);
    if (error != SaveError::None) return {error};
    return publish(root, m, budget, loaded.recovered_previous);
}
LoadResult load_save(const std::filesystem::path& root, const std::string& model, const std::string& render) {
    return load_any(root, model, render);
}
OutcomesResult read_outcomes(const std::filesystem::path& root, const std::string& model,
                             const std::string& render) {
    Manifest m;
    const auto loaded = load_any(root, model, render, &m);
    if (loaded.error != SaveError::None) return {loaded.error, {}, 0, 0};
    return outcomes_for(root, m);
}
OutcomesResult read_outcomes(const std::filesystem::path& root, const std::string& model,
                             const std::string& render, std::uint64_t generation) {
    const auto historical = load_history(root, generation, model, render);
    if (historical.error != SaveError::None) return {historical.error, {}, 0, 0};
    Manifest m;
    if (history_manifest(root, generation, m) != SaveError::None) {
        if (parse(root / "manifest", m) != SaveError::None || m.generation != generation ||
            verify_manifest(root, m) != SaveError::None) {
            if (parse(root / "manifest.previous", m) != SaveError::None ||
                m.generation != generation || verify_manifest(root, m) != SaveError::None)
                return {SaveError::NotFound, {}, 0, 0};
        }
    }
    return outcomes_for(root, m);
}
HistoryResult list_history(const std::filesystem::path& root) {
    HistoryResult result;
    Manifest current;
    result.error = parse(root / "manifest", current);
    if (result.error != SaveError::None || verify_manifest(root, current) != SaveError::None) {
        result.error = parse(root / "manifest.previous", current);
        if (result.error != SaveError::None || verify_manifest(root, current) != SaveError::None) {
            result.error = SaveError::Corrupt;
            return result;
        }
        result.recovered_previous = true;
    }
    result.first_second = current.second;
    result.last_second = current.second;
    for (const auto& entry : archives(root, current.generation)) {
        result.checkpoints.push_back({entry.generation, entry.revision, entry.second, entry.kind});
        result.first_second = std::min(result.first_second, entry.second);
    }
    if (result.checkpoints.empty() || result.checkpoints.back().generation != current.generation)
        result.checkpoints.push_back({current.generation, current.revision, current.second, current.kind});
    return result;
}
HistoricalView load_history(const std::filesystem::path& root, std::uint64_t generation,
                            const std::string& model, const std::string& render) {
    HistoricalView result;
    Manifest committed;
    Manifest current;
    if (parse(root / "manifest", current) == SaveError::None &&
        verify_manifest(root, current) == SaveError::None) {
        committed = current;
    } else if (parse(root / "manifest.previous", committed) != SaveError::None ||
               verify_manifest(root, committed) != SaveError::None) {
        result.error = SaveError::Corrupt;
        return result;
    }
    LoadResult loaded{SaveError::NotFound};
    for (const auto& historical : archives(root, committed.generation)) {
        if (historical.generation == generation) {
            loaded = load_manifest(root, historical, model, render);
            break;
        }
    }
    if (loaded.error == SaveError::NotFound && generation == committed.generation)
        loaded = load_manifest(root, committed, model, render);
    result.error = loaded.error;
    if (result.error == SaveError::None) {
        result.second = loaded.world->view().second;
        result.identity = std::move(loaded.identity);
        result.world = std::make_shared<const World>(std::move(*loaded.world));
    }
    return result;
}
SaveStatus create_branch(const std::filesystem::path& parent, const std::filesystem::path& branch,
                         Id new_branch, const std::string& model, const std::string& render) {
    std::error_code ec;
    if (new_branch == invalid_id || std::filesystem::exists(branch, ec)) return {SaveError::Incompatible};
    if (ec) return {SaveError::Io};
    Manifest source;
    auto loaded = load_any(parent, model, render, &source);
    if (loaded.error != SaveError::None) return {loaded.error};
    if (loaded.identity.branch == new_branch) return {SaveError::Incompatible};
    if (source.generation == UINT64_MAX || source.audit.size() + source.journals.size() > 65536)
        return {SaveError::CapacityExceeded};
    loaded.identity.parent_branch = loaded.identity.branch;
    loaded.identity.branch = new_branch;
    const auto captured = loaded.world->capture();
    const auto bytes = persistence::encode(captured, loaded.identity);
    if (bytes.size() > persistence::max_snapshot_bytes) return {SaveError::CapacityExceeded};
    const auto branch_snapshot = name('s', source.generation + 1);
    Manifest fork{source.generation + 1, captured.revision, captured.second,
                  CheckpointKind::Branch, branch_snapshot, {}, source.audit};
    fork.audit.insert(fork.audit.end(), source.journals.begin(), source.journals.end());
    std::set<std::string> required_origins;
    for (const auto& entry_name : fork.audit) {
        persistence::JournalProvenance provenance;
        const auto error = journal_provenance(parent, entry_name, provenance);
        if (error != SaveError::None) return {error};
        if (provenance.predecessor == provenance.checkpoint) {
            std::string origin;
            const auto origin_error = origin_snapshot(parent, entry_name, provenance.checkpoint, origin);
            if (origin_error != SaveError::None) return {origin_error};
            required_origins.insert(std::move(origin));
        }
    }
    struct Rollback {
        const std::filesystem::path& path;
        bool armed{true};
        ~Rollback() {
            if (armed) {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        }
    } rollback{branch};
    auto error = prepare(branch, 256ULL * 1024 * 1024);
    if (error != SaveError::None) return {error};
    error = persistence::write_artifact(branch / branch_snapshot, bytes, persistence::max_snapshot_bytes);
    if (error != SaveError::None) return {error};
    for (const auto& entry_name : fork.audit) {
        std::vector<std::uint8_t> entries;
        error = persistence::read_artifact(parent / entry_name, entries, persistence::max_journal_bytes);
        if (error != SaveError::None) return {error};
        error = persistence::write_artifact(branch / entry_name, entries, persistence::max_journal_bytes);
        if (error != SaveError::None) return {error};
    }
    for (const auto& filename : required_origins) {
        std::vector<std::uint8_t> origin;
        error = persistence::read_artifact(parent / filename, origin, persistence::max_snapshot_bytes);
        if (error != SaveError::None) return {error};
        error = persistence::write_artifact(branch / filename, origin, persistence::max_snapshot_bytes);
        if (error != SaveError::None) return {error};
    }
    const auto committed = publish(branch, fork, 256ULL * 1024 * 1024);
    if (committed.error == SaveError::None) rollback.armed = false;
    return committed;
}
SaveStatus migrate_save(const std::filesystem::path& source, const std::filesystem::path& destination,
                        const std::string& model, const std::string& render) {
    if (std::filesystem::exists(destination)) return {SaveError::Incompatible};
    std::error_code ec;
    const auto size = std::filesystem::file_size(source, ec);
    if (ec) return {SaveError::NotFound};
    if (size > 65536) return {SaveError::CapacityExceeded};
    std::ifstream file(source, std::ios::binary);
    std::string version;
    if (!(file >> version)) return {SaveError::Corrupt};
    if (version != "CFV1") return {SaveError::UnsupportedVersion};
    SaveIdentity tag;
    WorldLimits limits{};
    std::uint64_t seed;
    std::size_t count;
    if (!(file >> tag.campaign.high >> tag.campaign.low >> tag.branch.high >> tag.branch.low >>
          tag.source_campaign.high >> tag.source_campaign.low >> tag.source >> tag.model >> tag.render >>
          limits.max_entities >> limits.max_receipts >> limits.max_events >> limits.max_decisions >> seed >> count))
        return {SaveError::Corrupt};
    if (tag.model != model) return {SaveError::MissingModel};
    if (count == 0 || count > limits.max_entities || limits.max_entities > 4096 ||
        count > 4096 || limits.max_receipts > 65536 ||
        limits.max_events > 262144 || limits.max_decisions > 4096)
        return {SaveError::CapacityExceeded};
    std::vector<InitialControl> controls(count);
    for (auto& c : controls) if (!(file >> c.id.high >> c.id.low >> c.initial.value)) return {SaveError::Corrupt};
    if (!(file >> count) || count > limits.max_decisions) return {SaveError::Corrupt};
    std::vector<DecisionDeadline> deadlines(count);
    for (auto& d : deadlines)
        if (!(file >> d.id.high >> d.id.low >> d.entity_id.high >> d.entity_id.low >> d.second))
            return {SaveError::Corrupt};
    if (!(file >> count) || count > limits.max_receipts) return {SaveError::Corrupt};
    World world(limits);
    if (world.load(controls, deadlines, seed) != LoadError::None) return {SaveError::Corrupt};
    for (std::size_t i = 0; i < count; ++i) {
        Command c{};
        Id entity{};
        double requested;
        if (!(file >> c.header.command_id.high >> c.header.command_id.low >>
              c.header.actor_id.high >> c.header.actor_id.low >> entity.high >> entity.low >> requested))
            return {SaveError::Corrupt};
        c.header.expected_revision = world.view().revision;
        c.header.issued_at = world.view().second;
        c.action = SetControl{entity, Fraction{requested}};
        if (world.submit(c).error != SubmitError::None) return {SaveError::Corrupt};
    }
    SimSecond second;
    std::string extra;
    if (!(file >> second) || (file >> extra) || second < 0 || second > 86400 * 365 ||
        world.advance({second, static_cast<std::uint32_t>(second)}).reached_second != second)
        return {SaveError::Corrupt};
    // A render-only pack may be absent; physics never substitutes a different model.
    (void)render;
    return save_checkpoint(destination, world, tag, CheckpointKind::Manual);
}
} // namespace canopy
