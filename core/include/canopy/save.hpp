#pragma once
#include <canopy/world_state.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace canopy {
enum class SaveError {
    None, Io, Corrupt, UnsupportedVersion, MissingModel, CapacityExceeded,
    Incompatible, InvalidState, NotFound, BudgetExceeded
};
enum class CheckpointKind : std::uint8_t { Daily, Manual, Branch };
struct SaveIdentity {
    Id campaign{}, branch{}, source_campaign{};
    std::string source, model, render; // Caller-supplied immutable artifact digests, not invented hashes.
    Id parent_branch{}; // Empty on the origin; immutable fork lineage otherwise.
    SaveIdentity() = default;
    SaveIdentity(Id c, Id b, Id sc, std::string s, std::string m, std::string r, Id parent = {})
        : campaign(c), branch(b), source_campaign(sc), source(std::move(s)),
          model(std::move(m)), render(std::move(r)), parent_branch(parent) {}
};
struct SaveStatus { SaveError error{SaveError::None}; };
struct LoadResult {
    SaveError error{SaveError::None};
    std::optional<World> world;
    SaveIdentity identity;
    bool neutral_render{};
    bool recovered_previous{};
    LoadResult() = default;
    explicit LoadResult(SaveError e) : error(e) {}
};
struct HistoricalView {
    SaveError error{SaveError::None};
    std::shared_ptr<const World> world;
    SaveIdentity identity;
    SimSecond second{};
};
struct HistoryPoint {
    std::uint64_t generation{}, revision{};
    SimSecond second{};
    CheckpointKind kind{};
};
struct HistoryResult {
    SaveError error{SaveError::None};
    std::vector<HistoryPoint> checkpoints;
    bool recovered_previous{};
    SimSecond first_second{}, last_second{};
};
struct JournalEntry {
    enum class Kind : std::uint8_t { Submit, Advance } kind{};
    Command command{};
    SubmitResult outcome{};
    AdvanceBudget budget{};
    AdvanceResult progress{};
    static JournalEntry submission(Command c, SubmitResult r) { JournalEntry e; e.kind = Kind::Submit; e.command = c; e.outcome = r; return e; }
    static JournalEntry advance(AdvanceBudget b, AdvanceResult r) { JournalEntry e; e.kind = Kind::Advance; e.budget = b; e.progress = r; return e; }
};
struct OutcomesResult {
    SaveError error{SaveError::None};
    std::vector<JournalEntry> entries;
    std::uint64_t first_generation{}, last_generation{};
};
// Call capture/checkpoint/append only under the World owner's epoch. The writer owns
// the complete serialized bytes before any asynchronous handoff; never borrow World spans.
SaveStatus save_checkpoint(const std::filesystem::path& root, const World& world,
                           const SaveIdentity& identity, CheckpointKind kind = CheckpointKind::Daily,
                           std::uint64_t disk_budget = 256ULL * 1024 * 1024);
SaveStatus save_checkpoint(const std::filesystem::path& root, WorldState captured,
                           const SaveIdentity& identity, CheckpointKind kind = CheckpointKind::Daily,
                           std::uint64_t disk_budget = 256ULL * 1024 * 1024);
SaveStatus append_journal(const std::filesystem::path& root, std::span<const JournalEntry> entries,
                          std::uint64_t disk_budget = 256ULL * 1024 * 1024);
LoadResult load_save(const std::filesystem::path& root, const std::string& required_model,
                     const std::string& available_render);
HistoryResult list_history(const std::filesystem::path& root);
HistoricalView load_history(const std::filesystem::path& root, std::uint64_t generation,
                        const std::string& required_model, const std::string& available_render);
OutcomesResult read_outcomes(const std::filesystem::path& root, const std::string& required_model,
                             const std::string& available_render);
OutcomesResult read_outcomes(const std::filesystem::path& root, const std::string& required_model,
                             const std::string& available_render, std::uint64_t generation);
SaveStatus create_branch(const std::filesystem::path& parent, const std::filesystem::path& branch,
                         Id new_branch, const std::string& required_model,
                         const std::string& available_render);
SaveStatus migrate_save(const std::filesystem::path& source, const std::filesystem::path& destination,
                        const std::string& required_model, const std::string& available_render);
} // namespace canopy
