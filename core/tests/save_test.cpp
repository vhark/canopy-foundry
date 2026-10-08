#include "fixtures/clock_world.hpp"
#include <canopy/save.hpp>
#include "../src/persistence/internal.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <filesystem>
#include <fstream>

using namespace canopy;
namespace fs = std::filesystem;

TEST_CASE("Save: SHA-256 journal provenance matches a published digest vector", "[Save]") {
    constexpr std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    constexpr std::array<std::uint8_t, 32> expected{
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    REQUIRE(persistence::sha256(abc) == expected);
}

TEST_CASE("Save: corrupt newest checkpoint falls back with an explicit warning", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-corrupt";
    fs::remove_all(root);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE(world.submit(test::control()).error == SubmitError::None);
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    { std::ofstream out(root / "s-2", std::ios::binary | std::ios::trunc); out << "truncated"; }
    auto recovered = load_save(root, "m", "r");
    REQUIRE(recovered.error == SaveError::None);
    REQUIRE(recovered.recovered_previous);
    REQUIRE(recovered.world->view().revision == 0);
    REQUIRE(save_checkpoint(root, *recovered.world, identity).error == SaveError::None);
    { std::ofstream out(root / "s-3", std::ios::binary | std::ios::trunc); out << "truncated"; }
    auto twice = load_save(root, "m", "r");
    REQUIRE(twice.error == SaveError::None);
    REQUIRE(twice.recovered_previous);
    REQUIRE(twice.world->view().revision == 0);
    fs::remove_all(root);
}
TEST_CASE("Save: truncated committed journal recovers previous checkpoint with warning", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-truncated-journal";
    fs::remove_all(root);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto command = test::control();
    const auto receipt = world.submit(command);
    const std::array entries{JournalEntry::submission(command, receipt)};
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    { std::ofstream out(root / "j-2", std::ios::binary | std::ios::trunc); out << "bad"; }
    auto recovered = load_save(root, "m", "r");
    REQUIRE(recovered.error == SaveError::None);
    REQUIRE(recovered.recovered_previous);
    REQUIRE(recovered.world->view().revision == 0);
    fs::remove_all(root);
}

TEST_CASE("Save: orphan journal never blocks resumed writer and fallback history is honest", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-orphan-resume";
    fs::remove_all(root);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto command = test::control();
    const auto result = world.submit(command);
    const std::array transitions{JournalEntry::submission(command, result)};
    REQUIRE(append_journal(root, transitions).error == SaveError::None);
    { std::ofstream bad(root / "j-2", std::ios::trunc); bad << "interrupted"; }
    auto recovered = load_save(root, "m", "r");
    REQUIRE(recovered.error == SaveError::None);
    REQUIRE(recovered.recovered_previous);
    const auto history = list_history(root);
    REQUIRE(history.error == SaveError::None);
    REQUIRE(history.recovered_previous);
    REQUIRE(history.checkpoints.back().generation == 1);
    REQUIRE(load_history(root, 1, "m", "r").error == SaveError::None);
    REQUIRE(load_history(root, 2, "m", "r").error == SaveError::NotFound);
    const auto advanced = recovered.world->advance({5, 5});
    const std::array resumed{JournalEntry::advance({5, 5}, advanced)};
    REQUIRE(append_journal(root, resumed).error == SaveError::None);
    REQUIRE(load_save(root, "m", "r").world->view().second == 5);
    fs::remove_all(root);
}

TEST_CASE("Save: foreign branch journal cannot substitute a self-checksummed segment", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-foreign-root";
    const auto foreign = fs::temp_directory_path() / "canopy-save-foreign-branch";
    fs::remove_all(root); fs::remove_all(foreign);
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    auto parent = test::make_world();
    auto alternate = test::make_world();
    REQUIRE(save_checkpoint(root, parent, identity).error == SaveError::None);
    REQUIRE(create_branch(root, foreign, {4, 4}, "m", "r").error == SaveError::None);
    const auto a = test::control({0, 11}, 0.25);
    const auto b = test::control({0, 12}, 0.75);
    const std::array own{JournalEntry::submission(a, parent.submit(a))};
    const std::array theirs{JournalEntry::submission(b, alternate.submit(b))};
    REQUIRE(append_journal(root, own).error == SaveError::None);
    auto fork = load_save(foreign, "m", "r");
    REQUIRE(fork.error == SaveError::None);
    REQUIRE(append_journal(foreign, theirs).error == SaveError::None);
    fs::copy_file(foreign / "j-3", root / "j-2", fs::copy_options::overwrite_existing);
    auto loaded = load_save(root, "m", "r");
    REQUIRE(loaded.error == SaveError::None);
    REQUIRE(loaded.recovered_previous);
    REQUIRE(loaded.world->view().revision == 0);
    fs::remove_all(root); fs::remove_all(foreign);
}

TEST_CASE("Save: recovered writer appends after decodable-envelope corrupt current snapshot", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-decoded-recovery";
    fs::remove_all(root);
    auto world = test::make_world();
    const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    auto broken = world.capture();
    broken.limits.max_entities = 0;
    fs::remove(root / "s-2");
    REQUIRE(persistence::write_artifact(root / "s-2", persistence::encode(broken, identity),
                                        persistence::max_snapshot_bytes) == SaveError::None);
    auto recovered = load_save(root, "m", "r");
    REQUIRE(recovered.error == SaveError::None);
    REQUIRE(recovered.recovered_previous);
    const auto invalid = test::control(invalid_id);
    const std::array entries{JournalEntry::submission(invalid, recovered.world->submit(invalid))};
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    REQUIRE(load_save(root, "m", "r").error == SaveError::None);
    fs::remove_all(root);
}

TEST_CASE("Save: archived journal rejects identity replacement in later audit segment", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-audit-identity";
    fs::remove_all(root);
    auto world = test::make_world();
    const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto invalid = test::control(invalid_id);
    const std::array entries{JournalEntry::submission(invalid, world.submit(invalid))};
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    std::vector<std::uint8_t> initial;
    std::vector<std::uint8_t> origin;
    REQUIRE(persistence::read_artifact(root / "j-2", initial, persistence::max_journal_bytes) == SaveError::None);
    REQUIRE(persistence::read_artifact(root / "s-1", origin, persistence::max_snapshot_bytes) == SaveError::None);
    const persistence::JournalProvenance forged{{8, 8}, identity.branch,
        persistence::sha256(origin), persistence::sha256(initial)};
    fs::remove(root / "j-3");
    REQUIRE(persistence::write_artifact(root / "j-3",
        persistence::encode_journal(entries, 0, 0, forged),
        persistence::max_journal_bytes) == SaveError::None);
    const auto loaded = load_save(root, "m", "r");
    REQUIRE(loaded.error == SaveError::Corrupt);
    fs::remove_all(root);
}

TEST_CASE("Save: branch ignores unrelated corrupt ancestor snapshots", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-branch-ancestor";
    const auto branch = fs::temp_directory_path() / "canopy-save-branch-ancestor-child";
    fs::remove_all(root); fs::remove_all(branch);
    auto world = test::make_world();
    const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    { std::ofstream damaged(root / "s-1", std::ios::binary | std::ios::trunc); damaged << "damaged"; }
    REQUIRE(create_branch(root, branch, {4, 4}, "m", "r").error == SaveError::None);
    REQUIRE(load_save(branch, "m", "r").identity.branch == Id{4, 4});
    REQUIRE_FALSE(fs::exists(branch / "s-1"));
    fs::remove_all(root); fs::remove_all(branch);
}

TEST_CASE("Save: owned capture remains stable while authority advances before disk write", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-capture";
    fs::remove_all(root);
    auto world = test::make_world();
    const auto captured = world.capture();
    REQUIRE(world.submit(test::control()).error == SubmitError::None);
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, captured, identity).error == SaveError::None);
    auto loaded = load_save(root, "m", "r");
    REQUIRE(loaded.error == SaveError::None);
    REQUIRE(loaded.world->view().revision == 0);
    REQUIRE(world.view().revision == 1);
    fs::remove_all(root);
}

TEST_CASE("Save: checkpoint restores real control, deadlines, accepted receipt and events", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-roundtrip";
    fs::remove_all(root);
    auto world = test::make_world();
    const auto first = world.submit(test::control());
    REQUIRE(first.error == SubmitError::None);
    REQUIRE(world.advance({60, 60}).stop == AdvanceStop::DecisionRequired);
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "source-sha256", "model-sha256", "render-sha256"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    auto loaded = load_save(root, "model-sha256", "render-sha256");
    REQUIRE(loaded.error == SaveError::None);
    REQUIRE(loaded.world.has_value());
    REQUIRE(loaded.world->view().second == 60);
    REQUIRE(loaded.world->view().decision_pending);
    REQUIRE(loaded.world->view().controls[0] == world.view().controls[0]);
    REQUIRE(loaded.world->events().size() == world.events().size());
    REQUIRE(loaded.world->submit(test::control(test::first_command, 0.75, 99, 61)) == first);
    REQUIRE(loaded.world->submit(test::control(test::first_command, 0.5)).error == SubmitError::Conflict);
    REQUIRE(loaded.world->submit(test::resolve({0, 2}, 1, 60)).error == SubmitError::None);
    fs::remove_all(root);
}

TEST_CASE("Save: journal recovers accepted and rejected outcomes and exact elapsed time", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-journal";
    fs::remove_all(root);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    auto command = test::control();
    auto accepted = world.submit(command);
    auto rejected = world.submit(test::control({0, 9}, 0.5, 0));
    REQUIRE(rejected.error == SubmitError::StaleRevision);
    auto advanced = world.advance({60, 60});
    const std::array transitions{JournalEntry::submission(command, accepted),
                                 JournalEntry::submission(test::control({0, 9}, 0.5, 0), rejected),
                                 JournalEntry::advance({60, 60}, advanced)};
    REQUIRE(append_journal(root, transitions).error == SaveError::None);
    auto loaded = load_save(root, "m", "missing-render");
    REQUIRE(loaded.error == SaveError::None);
    REQUIRE(loaded.neutral_render);
    REQUIRE(loaded.world->view().second == world.view().second);
    REQUIRE(loaded.world->receipts()[0] == accepted.receipt);
    REQUIRE(loaded.world->events().size() == world.events().size());
    auto outcomes = read_outcomes(root, "m", "r");
    REQUIRE(outcomes.error == SaveError::None);
    REQUIRE(outcomes.entries.size() == transitions.size());
    REQUIRE(outcomes.entries[1].outcome == rejected);
    REQUIRE(save_checkpoint(root, world, identity, CheckpointKind::Manual).error == SaveError::None);
    outcomes = read_outcomes(root, "m", "r");
    REQUIRE(outcomes.error == SaveError::None);
    REQUIRE(outcomes.entries.size() == transitions.size());
    REQUIRE(outcomes.entries[1].outcome == rejected);
    const auto branch = fs::temp_directory_path() / "canopy-save-audit-branch";
    fs::remove_all(branch);
    REQUIRE(create_branch(root, branch, {4, 4}, "m", "r").error == SaveError::None);
    const auto fork_outcomes = read_outcomes(branch, "m", "r");
    REQUIRE(fork_outcomes.error == SaveError::None);
    REQUIRE(fork_outcomes.entries.size() == transitions.size());
    REQUIRE(fork_outcomes.entries[1].outcome == rejected);
    fs::remove_all(branch);
    REQUIRE(load_save(root, "wrong-model", "r").error == SaveError::MissingModel);
    fs::remove_all(root);
}

TEST_CASE("Save: history and branch retain source while isolating later writes", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-history";
    auto branch = fs::temp_directory_path() / "canopy-save-branch";
    fs::remove_all(root); fs::remove_all(branch);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE(world.submit(test::control()).error == SubmitError::None);
    REQUIRE(save_checkpoint(root, world, identity, CheckpointKind::Manual).error == SaveError::None);
    const auto history = list_history(root);
    REQUIRE(history.error == SaveError::None);
    REQUIRE(history.checkpoints.size() == 2);
    auto prior = load_history(root, history.checkpoints.front().generation, "m", "r");
    REQUIRE(prior.error == SaveError::None);
    REQUIRE(prior.world->view().revision == 0);
    REQUIRE(create_branch(root, branch, {4, 4}, "m", "r").error == SaveError::None);
    auto fork = load_save(branch, "m", "r");
    REQUIRE(fork.identity.branch == Id{4, 4});
    REQUIRE(fork.identity.parent_branch == identity.branch);
    REQUIRE(fork.identity.campaign == identity.campaign);
    REQUIRE(fork.identity.source == identity.source);
    REQUIRE(load_save(root, "m", "r").identity.branch == identity.branch);
    fs::remove_all(root); fs::remove_all(branch);
}

TEST_CASE("Save: rolling retention keeps seven daily checkpoints and never deletes manual", "[Save]") {
    auto root = fs::temp_directory_path() / "canopy-save-retention";
    fs::remove_all(root);
    auto world = test::make_world();
    SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity, CheckpointKind::Manual).error == SaveError::None);
    for (int day = 1; day <= 9; ++day)
        REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto history = list_history(root);
    REQUIRE(history.error == SaveError::None);
    REQUIRE(history.checkpoints.size() == 8);
    REQUIRE(history.checkpoints.front().kind == CheckpointKind::Manual);
    REQUIRE(load_history(root, history.checkpoints.front().generation, "m", "r").error == SaveError::None);
    fs::remove_all(root);
}

TEST_CASE("Save: expired daily audit artifacts are collected after archival readers retire", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-daily-collection";
    fs::remove_all(root);
    auto world = test::make_world();
    const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto invalid = test::control(invalid_id);
    const std::array entries{JournalEntry::submission(invalid, world.submit(invalid))};
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    for (int day = 0; day < 16; ++day)
        REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE_FALSE(fs::exists(root / "j-2"));
    REQUIRE_FALSE(fs::exists(root / "s-1"));
    REQUIRE(list_history(root).checkpoints.size() == 7);
    REQUIRE(read_outcomes(root, "m", "r").entries.empty());
    REQUIRE(append_journal(root, entries).error == SaveError::None);
    REQUIRE(read_outcomes(root, "m", "r").first_generation > 1);
    fs::remove_all(root);
}

TEST_CASE("Save: journal retention advances beyond 512 while manual replay pins origins", "[Save]") {
    const auto root = fs::temp_directory_path() / "canopy-save-long-retention";
    fs::remove_all(root);
    auto world = test::make_world();
    const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "s", "m", "r"};
    REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    const auto invalid = test::control(invalid_id);
    const auto rejected = world.submit(invalid);
    REQUIRE(rejected.error == SubmitError::InvalidId);
    const std::array transitions{JournalEntry::submission(invalid, rejected)};
    for (int i = 0; i < 513; ++i)
        REQUIRE(append_journal(root, transitions).error == SaveError::None);
    REQUIRE(save_checkpoint(root, world, identity, CheckpointKind::Manual).error == SaveError::None);
    const auto before = list_history(root);
    REQUIRE(before.error == SaveError::None);
    const auto manual_generation = before.checkpoints.back().generation;
    for (int i = 0; i < 9; ++i)
        REQUIRE(save_checkpoint(root, world, identity).error == SaveError::None);
    REQUIRE(load_history(root, manual_generation, "m", "r").error == SaveError::None);
    const auto manual_outcomes = read_outcomes(root, "m", "r", manual_generation);
    REQUIRE(manual_outcomes.error == SaveError::None);
    REQUIRE(manual_outcomes.entries.size() == 513);
    REQUIRE(fs::exists(root / "s-1"));
    REQUIRE(append_journal(root, transitions).error == SaveError::None);
    const auto latest = read_outcomes(root, "m", "r");
    REQUIRE(latest.error == SaveError::None);
    REQUIRE(latest.entries.size() < manual_outcomes.entries.size());
    REQUIRE(latest.first_generation > 1);
    fs::remove_all(root);
}
