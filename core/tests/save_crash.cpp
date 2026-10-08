#include "fixtures/clock_world.hpp"
#include <canopy/save.hpp>
#include "../src/persistence/internal.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace canopy;
namespace fs = std::filesystem;
namespace {
const SaveIdentity identity{{1, 1}, {2, 2}, {3, 3}, "source-crash", "model-crash", "render-crash"};
int child_write(const fs::path& root) {
    auto loaded = load_save(root, identity.model, identity.render);
    if (loaded.error != SaveError::None) return 3;
    auto& world = *loaded.world;
    if (world.submit(test::control()).error != SubmitError::None ||
        world.advance({60, 60}).stop != AdvanceStop::DecisionRequired) return 4;
    return save_checkpoint(root, world, identity).error == SaveError::None ? 0 : 5;
}
int child_journal(const fs::path& root, bool after_recovery) {
    auto loaded = load_save(root, identity.model, identity.render);
    if (loaded.error != SaveError::None || (after_recovery && !loaded.recovered_previous)) return 20;
    if (after_recovery) {
        const auto progress = loaded.world->advance({5, 5});
        const std::array entries{JournalEntry::advance({5, 5}, progress)};
        return append_journal(root, entries).error == SaveError::None ? 0 : 21;
    }
    const auto command = test::control();
    const auto outcome = loaded.world->submit(command);
    const std::array entries{JournalEntry::submission(command, outcome)};
    return append_journal(root, entries).error == SaveError::None ? 0 : 22;
}
int qualification(const fs::path& executable, const fs::path& root) {
    constexpr std::array stages{"data-written", "data-flushed", "pre-manifest", "post-manifest", "directory-flush"};
    constexpr std::array modes{"write", "journal", "recover-journal"};
    for (const auto* mode : modes) for (const auto* stage : stages) {
        fs::remove_all(root);
        auto world = test::make_world();
        if (save_checkpoint(root, world, identity).error != SaveError::None) return 10;
        if (std::string(mode) == "recover-journal") {
            const auto command = test::control();
            const std::array initial{JournalEntry::submission(command, world.submit(command))};
            if (append_journal(root, initial).error != SaveError::None) return 23;
            { std::ofstream damaged(root / "j-2", std::ios::binary | std::ios::trunc); damaged << "truncated"; }
            if (!load_save(root, identity.model, identity.render).recovered_previous) return 24;
        }
        const auto signal = root / "stage.signal";
#ifdef _WIN32
        _putenv_s("CANOPY_SAVE_KILL_STAGE", stage);
        _putenv_s("CANOPY_SAVE_STAGE_FILE", signal.string().c_str());
        std::wstring command = L"\"" + executable.wstring() + L"\" " +
            fs::path(mode).wstring() + L" \"" + root.wstring() + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) return 11;
#else
        const auto child = fork();
        if (child < 0) return 11;
        if (child == 0) {
            setenv("CANOPY_SAVE_KILL_STAGE", stage, 1);
            setenv("CANOPY_SAVE_STAGE_FILE", signal.c_str(), 1);
            execl(executable.c_str(), executable.c_str(), mode, root.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
#endif
        bool reached = false;
        for (int attempt = 0; attempt < 300; ++attempt) {
            if (fs::exists(signal)) { reached = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
#ifdef _WIN32
        TerminateProcess(process.hProcess, 137);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        _putenv_s("CANOPY_SAVE_KILL_STAGE", "");
        _putenv_s("CANOPY_SAVE_STAGE_FILE", "");
#else
        kill(child, SIGKILL);
        int status = 0; waitpid(child, &status, 0);
#endif
        if (!reached) return 12;
        auto loaded = load_save(root, identity.model, identity.render);
        if (loaded.error != SaveError::None) return 13;
        const bool committed = std::string(stage) == "post-manifest" || std::string(stage) == "directory-flush";
        auto expected = test::make_world();
        if (committed) {
            if (std::string(mode) == "write") {
                if (expected.submit(test::control()).error != SubmitError::None ||
                    expected.advance({60, 60}).stop != AdvanceStop::DecisionRequired) return 14;
            } else if (std::string(mode) == "journal") {
                if (expected.submit(test::control()).error != SubmitError::None) return 14;
            } else {
                if (expected.advance({5, 5}) != AdvanceResult{5, AdvanceStop::ReachedTarget}) return 14;
            }
        }
        const auto digest = [](const World& authority) {
            return persistence::sha256(persistence::encode(authority.capture(), identity));
        };
        if (digest(*loaded.world) != digest(expected)) return 14;
        const auto invalid = test::control(invalid_id);
        const auto rejected = loaded.world->submit(invalid);
        const std::array continuation{JournalEntry::submission(invalid, rejected)};
        if (rejected.error != SubmitError::InvalidId ||
            append_journal(root, continuation).error != SaveError::None) return 25;
        auto resumed = load_save(root, identity.model, identity.render);
        if (resumed.error != SaveError::None || digest(*resumed.world) != digest(expected)) return 26;
        if (committed && std::string(mode) != "recover-journal") {
            const auto duplicate = test::control();
            if (resumed.world->submit(duplicate) != expected.submit(duplicate) ||
                digest(*resumed.world) != digest(expected)) return 28;
        }
        const auto outcomes = read_outcomes(root, identity.model, identity.render);
        if (outcomes.error != SaveError::None || outcomes.entries.empty() ||
            outcomes.entries.back().kind != JournalEntry::Kind::Submit ||
            outcomes.entries.back().command != invalid ||
            outcomes.entries.back().outcome != rejected) return 27;
        std::cout << mode << '/' << stage << " recovered and resumed full authority digest" << '\n';
    }
    fs::remove_all(root);
    return 0;
}
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    if (std::string(argv[1]) == "write") return child_write(argv[2]);
    if (std::string(argv[1]) == "journal") return child_journal(argv[2], false);
    if (std::string(argv[1]) == "recover-journal") return child_journal(argv[2], true);
    if (std::string(argv[1]) == "qualify") return qualification(fs::absolute(argv[0]), fs::absolute(argv[2]));
    return 2;
}
