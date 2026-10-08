#include "fixtures/clock_world.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <new>

namespace {
thread_local bool observe_allocations = false;
thread_local std::size_t allocation_count = 0;

struct AllocationScope {
    AllocationScope() { allocation_count = 0; observe_allocations = true; }
    ~AllocationScope() { observe_allocations = false; }
    [[nodiscard]] std::size_t count() const { return allocation_count; }
};
} // namespace

void* operator new(std::size_t size) {
    if (observe_allocations) ++allocation_count;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    if (observe_allocations) ++allocation_count;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

using namespace canopy;

TEST_CASE("sixty single-second advances replay the same authoritative state and events", "[replay]") {
    auto full = test::make_world();
    auto segmented = test::make_world();
    const auto command = test::control();
    REQUIRE(full.submit(command) == segmented.submit(command));
    REQUIRE(full.advance({60, 60}) == AdvanceResult{60, AdvanceStop::DecisionRequired});
    for (SimSecond target = 1; target <= 60; ++target) {
        const auto outcome = segmented.advance({target, 1});
        REQUIRE(outcome.reached_second == target);
        REQUIRE(outcome.stop == (target == 60 ? AdvanceStop::DecisionRequired : AdvanceStop::ReachedTarget));
    }
    REQUIRE(full.view().revision == segmented.view().revision);
    REQUIRE(full.view().climate_boundaries == segmented.view().climate_boundaries);
    REQUIRE(full.view().crop_boundaries == segmented.view().crop_boundaries);
    REQUIRE(full.view().controls[0] == segmented.view().controls[0]);
    REQUIRE(full.events().size() == segmented.events().size());
    for (std::size_t i = 0; i < full.events().size(); ++i) REQUIRE(full.events()[i] == segmented.events()[i]);
}

TEST_CASE("random streams separate entity, stream and counter domains without unsolicited draws", "[replay]") {
    World world({2, 8, 8});
    constexpr std::array controls{InitialControl{{0, 2}, Fraction{0.25}}, InitialControl{{0, 1}, Fraction{0.5}}};
    REQUIRE(world.load(controls, {}, 7) == LoadError::None);
    const auto sample_before = world.sample({0, 2}, 13, 42);
    REQUIRE(world.advance({5, 5}) == AdvanceResult{5, AdvanceStop::ReachedTarget});
    REQUIRE(world.view().controls[0].id == Id{0, 1});
    REQUIRE(world.sample({0, 1}, 2, 7) != world.sample({0, 1}, 7, 2));
    REQUIRE(world.sample({0, 1}, 3, 3) != world.sample({0, 1}, 4, 4));
    REQUIRE(world.sample({0, 1}, 2, 7) != world.sample({0, 2}, 2, 7));
    REQUIRE(world.sample({0, 2}, 13, 42) == sample_before);
}

TEST_CASE("same-time events order by phase and stable entity ID, not delivery order", "[replay]") {
    World world({2, 8, 8});
    constexpr std::array controls{InitialControl{{0, 2}, Fraction{0.2}}, InitialControl{{0, 1}, Fraction{0.2}}};
    REQUIRE(world.load(controls, {}, 5) == LoadError::None);
    REQUIRE(world.submit(test::control({0, 20}, 0.7, 0, 0, {0, 2})).error == SubmitError::None);
    REQUIRE(world.submit(test::control({0, 10}, 0.6, 1, 0, {0, 1})).error == SubmitError::None);
    REQUIRE(world.events()[0].entity == Id{0, 1});
    REQUIRE(world.events()[1].entity == Id{0, 2});
    REQUIRE(world.advance({1, 1}).stop == AdvanceStop::ReachedTarget);
    REQUIRE(world.events()[2].type == EventType::ControlApplied);
    REQUIRE(world.events()[2].entity == Id{0, 1});
    REQUIRE(world.events()[3].entity == Id{0, 2});
}

TEST_CASE("admitted command delivery and clock stepping allocate nothing after load", "[replay]") {
    auto world = test::make_world();
    std::size_t observed_calibration = 0;
    {
        AllocationScope calibration;
        void* memory = ::operator new(8);
        ::operator delete(memory);
        observed_calibration = calibration.count();
    }
    REQUIRE(observed_calibration == 1);
    const auto command = test::control();
    std::size_t count = 0;
    SubmitResult first{};
    SubmitResult repeated{};
    AdvanceResult advanced{};
    SubmitResult resolved{};
    AdvanceResult resumed{};
    {
        AllocationScope scope;
        first = world.submit(command);
        repeated = world.submit(command);
        advanced = world.advance({60, 60});
        resolved = world.submit(test::resolve({0, 2}, 1, 60));
        resumed = world.advance({120, 60});
        count = scope.count();
    }
    REQUIRE(first.error == SubmitError::None);
    REQUIRE(first == repeated);
    REQUIRE(advanced == AdvanceResult{60, AdvanceStop::DecisionRequired});
    REQUIRE(resolved.error == SubmitError::None);
    REQUIRE(resumed == AdvanceResult{120, AdvanceStop::DecisionRequired});
    REQUIRE(count == 0);
}
