#pragma once
#include <canopy/save.hpp>
#include <array>
#include <span>

namespace canopy::persistence {
constexpr std::size_t max_snapshot_bytes = 64U * 1024U * 1024U;
constexpr std::size_t max_journal_bytes = 16U * 1024U * 1024U;
using Digest = std::array<std::uint8_t, 32>;
struct JournalProvenance {
    Id campaign{}, branch{};
    Digest checkpoint{}, predecessor{};
};
Digest sha256(std::span<const std::uint8_t>);
std::vector<std::uint8_t> encode(const WorldState&, const SaveIdentity&);
SaveError decode(std::span<const std::uint8_t>, WorldState&, SaveIdentity&);
std::vector<std::uint8_t> encode_journal(std::span<const JournalEntry>, std::uint64_t, SimSecond,
                                         const JournalProvenance&);
SaveError decode_journal(std::span<const std::uint8_t>, std::vector<JournalEntry>&,
                         std::uint64_t&, SimSecond&, JournalProvenance&);
SaveError replay_journal(std::span<const std::uint8_t>, World&, const JournalProvenance&);
SaveError write_artifact(const std::filesystem::path&, std::span<const std::uint8_t>, std::size_t);
SaveError read_artifact(const std::filesystem::path&, std::vector<std::uint8_t>&, std::size_t);
SaveError atomic_manifest(const std::filesystem::path&, std::span<const std::uint8_t>,
                          bool preserve_previous = false);
void durability_stage(const char* name);
} // namespace canopy::persistence
