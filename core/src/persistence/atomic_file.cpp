#include "internal.hpp"
#include <zstd.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace canopy::persistence {
namespace {
void put32(std::uint8_t* p, std::uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(n >> (i * 8)); }
std::uint32_t get32(const std::uint8_t* p) { return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24); }
// CRC-32/ISO-HDLC, reflected polynomial 0xEDB88320, init/final xor 0xFFFFFFFF.
std::uint32_t crc(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = 0xffffffffU;
    for (auto byte : bytes) {
        value ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1U) ? 0xedb88320U : 0U);
    }
    return ~value;
}
SaveError write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return SaveError::Io;
    bool ok = true;
    for (std::size_t off = 0; off < data.size() && ok;) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - off, 1U << 20));
        DWORD written = 0;
        ok = WriteFile(handle, data.data() + off, chunk, &written, nullptr) && written == chunk;
        off += written;
    }
    durability_stage("data-written");
    if (ok) ok = FlushFileBuffers(handle) != 0;
    if (!CloseHandle(handle)) ok = false;
    durability_stage("data-flushed");
    return ok ? SaveError::None : SaveError::Io;
#else
    const int fd = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) return SaveError::Io;
    bool ok = true;
    for (std::size_t off = 0; off < data.size() && ok;) {
        const auto written = write(fd, data.data() + off, data.size() - off);
        if (written <= 0) ok = false;
        else off += static_cast<std::size_t>(written);
    }
    durability_stage("data-written");
    if (ok && fsync(fd) != 0) ok = false;
    if (close(fd) != 0) ok = false;
    durability_stage("data-flushed");
    return ok ? SaveError::None : SaveError::Io;
#endif
}
SaveError sync_directory(const std::filesystem::path& dir) {
#ifdef _WIN32
    // MoveFileExW WRITE_THROUGH is the qualified Windows namespace durability primitive.
    (void)dir;
    return SaveError::None;
#else
    const int fd = open(dir.c_str(), O_RDONLY);
    if (fd < 0) return SaveError::Io;
    const bool ok = fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    return ok && closed ? SaveError::None : SaveError::Io;
#endif
}
SaveError replace(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return SaveError::Io;
#else
    if (rename(from.c_str(), to.c_str()) != 0) return SaveError::Io;
#endif
    if (to.filename() == "manifest") durability_stage("post-manifest");
    return sync_directory(to.parent_path());
}
}
void durability_stage(const char* name) {
    // Only the qualification helper explicitly arms this hook. A real external process
    // kills this process after observing the ready sentinel; it is not an I/O mock.
#ifdef _WIN32
    char stage[32]{};
    const DWORD stage_size = GetEnvironmentVariableA("CANOPY_SAVE_KILL_STAGE", stage, sizeof(stage));
    if (!stage_size || stage_size >= sizeof(stage) || std::strcmp(stage, name)) return;
    const DWORD required = GetEnvironmentVariableW(L"CANOPY_SAVE_STAGE_FILE", nullptr, 0);
    if (!required) return;
    std::wstring signal(required, L'\0');
    const DWORD copied = GetEnvironmentVariableW(L"CANOPY_SAVE_STAGE_FILE", signal.data(), required);
    if (!copied || copied >= required) return;
    signal.resize(copied);
#else
    const char* stage = std::getenv("CANOPY_SAVE_KILL_STAGE");
    if (!stage || std::strcmp(stage, name)) return;
    const char* signal = std::getenv("CANOPY_SAVE_STAGE_FILE");
    if (!signal) return;
#endif
    std::ofstream out(std::filesystem::path(signal), std::ios::binary | std::ios::trunc);
    out << name << '\n'; out.flush(); out.close();
    for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
SaveError write_artifact(const std::filesystem::path& path, std::span<const std::uint8_t> bytes, std::size_t limit) {
    if (bytes.size() > limit || bytes.empty()) return SaveError::CapacityExceeded;
    std::vector<std::uint8_t> packed(20 + ZSTD_compressBound(bytes.size()));
    const auto size = ZSTD_compress(packed.data() + 20, packed.size() - 20, bytes.data(), bytes.size(), 3);
    if (ZSTD_isError(size) || size > limit || size > UINT32_MAX || bytes.size() > UINT32_MAX) return SaveError::CapacityExceeded;
    packed.resize(20 + size);
    std::memcpy(packed.data(), "CFZ2", 4);
    put32(packed.data() + 4, static_cast<std::uint32_t>(bytes.size()));
    put32(packed.data() + 8, static_cast<std::uint32_t>(size));
    put32(packed.data() + 12, crc(bytes));
    put32(packed.data() + 16, crc(std::span<const std::uint8_t>(packed.data() + 20, size)));
    return write_file(path, packed);
}
SaveError read_artifact(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, std::size_t limit) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return SaveError::NotFound;
    if (size < 20 || size > limit + 20) return SaveError::Corrupt;
    std::vector<std::uint8_t> packed(static_cast<std::size_t>(size));
    std::ifstream in(path, std::ios::binary);
    if (!in.read(reinterpret_cast<char*>(packed.data()), static_cast<std::streamsize>(size))) return SaveError::Corrupt;
    if (std::memcmp(packed.data(), "CFZ2", 4) || get32(packed.data() + 4) > limit ||
        get32(packed.data() + 8) != size - 20 || get32(packed.data() + 4) == 0 ||
        crc(std::span<const std::uint8_t>(packed.data() + 20, size - 20)) != get32(packed.data() + 16)) return SaveError::Corrupt;
    bytes.resize(get32(packed.data() + 4));
    const auto decompressed = ZSTD_decompress(bytes.data(), bytes.size(), packed.data() + 20, size - 20);
    if (ZSTD_isError(decompressed) || decompressed != bytes.size() || crc(bytes) != get32(packed.data() + 12)) return SaveError::Corrupt;
    return SaveError::None;
}
SaveError atomic_manifest(const std::filesystem::path& root, std::span<const std::uint8_t> data,
                          bool preserve_previous) {
    const auto current = root / "manifest";
    const auto previous = root / "manifest.previous";
    const auto temp = root / "manifest.tmp";
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    if (!preserve_previous && std::filesystem::exists(current, ec)) {
        std::ifstream in(current, std::ios::binary);
        std::vector<std::uint8_t> old((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (old.size() > 4U * 1024U * 1024U || old.empty()) return SaveError::Corrupt;
        if (write_file(temp, old) != SaveError::None) return SaveError::Io;
        if (replace(temp, previous) != SaveError::None) return SaveError::Io;
    }
    if (write_file(temp, data) != SaveError::None) return SaveError::Io;
    durability_stage("pre-manifest");
    const auto replaced = replace(temp, current);
    if (replaced != SaveError::None) return replaced;
    // The rename was completed in replace(); this point is after directory durability.
    durability_stage("directory-flush");
    return SaveError::None;
}
} // namespace canopy::persistence
