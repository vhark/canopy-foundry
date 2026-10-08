#include <canopy/save.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace canopy;
namespace fs = std::filesystem;
TEST_CASE("Migration: original version one fixture migrates without modifying its bytes", "[Migration]") {
    const fs::path fixture = CANOPY_SAVE_FIXTURE_ROOT "/v1/original.save";
    std::ifstream input(fixture, std::ios::binary);
    const std::string original((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE_FALSE(original.empty());
    auto destination = fs::temp_directory_path() / "canopy-v1-forward";
    fs::remove_all(destination);
    REQUIRE(migrate_save(fixture, destination, "model-v1", "render-v1").error == SaveError::None);
    auto restored = load_save(destination, "model-v1", "render-v1");
    REQUIRE(restored.error == SaveError::None);
    REQUIRE(restored.world->view().second == 60);
    REQUIRE(restored.world->view().revision == 1);
    REQUIRE(restored.world->view().decision_pending);
    std::ifstream after(fixture, std::ios::binary);
    REQUIRE(std::string((std::istreambuf_iterator<char>(after)), std::istreambuf_iterator<char>()) == original);
    fs::remove_all(destination);
}
TEST_CASE("Migration: legacy capacity declarations are bounded before allocation", "[Migration]") {
    const fs::path fixture = CANOPY_SAVE_FIXTURE_ROOT "/v1/original.save";
    std::ifstream input(fixture, std::ios::binary);
    std::string original((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto position = original.find("2 32 32 32 41");
    REQUIRE(position != std::string::npos);
    original.replace(position, 13, "4294967296 32 32 32 41");
    const auto source = fs::temp_directory_path() / "canopy-v1-overlarge";
    const auto destination = fs::temp_directory_path() / "canopy-v1-overlarge-migrated";
    fs::remove_all(destination);
    { std::ofstream out(source); out << original; }
    REQUIRE(migrate_save(source, destination, "model-v1", "render-v1").error == SaveError::CapacityExceeded);
    REQUIRE_FALSE(fs::exists(destination));
    fs::remove(source);
}

TEST_CASE("Migration: unknown historical version refuses writes", "[Migration]") {
    auto source = fs::temp_directory_path() / "canopy-unknown-version";
    auto destination = fs::temp_directory_path() / "canopy-unknown-migrated";
    fs::remove_all(destination);
    { std::ofstream out(source); out << "CFV99\n"; }
    REQUIRE(migrate_save(source, destination, "model-v1", "render-v1").error == SaveError::UnsupportedVersion);
    REQUIRE_FALSE(fs::exists(destination));
    fs::remove(source);
}
