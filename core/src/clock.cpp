#include <canopy/clock.hpp>

namespace canopy {
bool is_boundary(SimSecond second, SimSecond interval) noexcept {
    return second > 0 && interval > 0 && second % interval == 0;
}

namespace {
std::uint64_t mix(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
} // namespace

std::uint64_t random_sample(std::uint64_t seed, Id entity,
                            std::uint64_t stream, std::uint64_t counter) noexcept {
    std::uint64_t state = mix(seed ^ 0x623f3dd9156af637ULL);
    state = mix(state ^ mix(entity.high ^ 0xc33dd5c6f7ab9a41ULL));
    state = mix(state ^ mix(entity.low ^ 0xd6e8feb86659fd93ULL));
    state = mix(state ^ mix(stream ^ 0xa0761d6478bd642fULL));
    return mix(state ^ mix(counter ^ 0xe7037ed1a0b428dbULL));
}
} // namespace canopy
