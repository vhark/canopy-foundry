#include "fixtures/clock_world.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace canopy;

TEST_CASE("one request to 120 stops at the exact 60 second decision and resumes", "[clock]") {
    auto world = test::make_world();
    REQUIRE(world.submit(test::control()).receipt.revision == 1);
    REQUIRE(world.submit(test::control()).receipt.revision == 1);
    REQUIRE(world.advance({120, 120}) == AdvanceResult{60, AdvanceStop::DecisionRequired});
    REQUIRE(world.view().controls[0].applied.value == 0.75);
    REQUIRE(world.view().climate_boundaries == 12);
    REQUIRE(world.view().crop_boundaries == 1);
    REQUIRE(world.view().decision_pending);
    REQUIRE(world.advance({120, 120}) == AdvanceResult{60, AdvanceStop::DecisionRequired});
    REQUIRE(world.submit(test::resolve({0, 2}, 1, 60)).receipt.revision == 2);
    REQUIRE(world.events()[15].type == EventType::DecisionRequired);
    REQUIRE(world.events()[16].type == EventType::DecisionResolved);
    REQUIRE(world.advance({120, 120}) == AdvanceResult{120, AdvanceStop::DecisionRequired});
    REQUIRE(world.view().climate_boundaries == 24);
    REQUIRE(world.view().crop_boundaries == 2);
}

TEST_CASE("budgets yield without skipping a one second boundary", "[clock]") {
    auto world = test::make_world();
    REQUIRE(world.submit(test::control()).error == SubmitError::None);
    REQUIRE(world.advance({60, 0}) == AdvanceResult{0, AdvanceStop::BudgetExhausted});
    REQUIRE(world.view().controls[0].applied.value == 0.25);
    REQUIRE(world.advance({60, 1}) == AdvanceResult{1, AdvanceStop::BudgetExhausted});
    REQUIRE(world.view().controls[0].applied.value == 0.75);
    REQUIRE(world.advance({60, 59}) == AdvanceResult{60, AdvanceStop::DecisionRequired});
}

TEST_CASE("invalid targets cannot change the clock, even near integer limits", "[clock]") {
    auto world = test::make_world();
    REQUIRE(world.advance({-1, 10}) == AdvanceResult{0, AdvanceStop::InvalidModel});
    REQUIRE(world.advance({std::numeric_limits<SimSecond>::max(), 1}) == AdvanceResult{1, AdvanceStop::BudgetExhausted});
    REQUIRE(world.advance({0, 10}) == AdvanceResult{1, AdvanceStop::InvalidModel});
    REQUIRE(world.view().second == 1);
    REQUIRE_FALSE(is_boundary(std::numeric_limits<SimSecond>::max(), 60));
    REQUIRE(random_sample(7, test::room, 2, 7) != random_sample(7, test::room, 7, 2));
}

TEST_CASE("event capacity rejects a whole step before changing time or control", "[clock]") {
    auto world = test::make_world({1, 4, 1});
    REQUIRE(world.submit(test::control()).error == SubmitError::None);
    REQUIRE(world.advance({1, 1}) == AdvanceResult{0, AdvanceStop::CapacityExhausted});
    REQUIRE(world.view().second == 0);
    REQUIRE(world.view().controls[0].applied.value == 0.25);
    REQUIRE(world.events().size() == 1);
}

TEST_CASE("load rejects duplicate entities, invalid fractions and insufficient capacity", "[clock]") {
    World world({1, 4, 4});
    const std::array duplicates{InitialControl{test::room, Fraction{0.2}}, InitialControl{test::room, Fraction{0.3}}};
    REQUIRE(world.load(duplicates, {}, 1) == LoadError::CapacityExceeded);
    World another({2, 4, 4});
    REQUIRE(another.load(duplicates, {}, 1) == LoadError::DuplicateEntity);
    const std::array invalid{InitialControl{test::room, Fraction{1.1}}};
    REQUIRE(another.load(invalid, {}, 1) == LoadError::InvalidValue);
    REQUIRE(another.view().controls.empty());
    World impossible({1, std::numeric_limits<std::size_t>::max(), 4});
    const std::array one{InitialControl{test::room, Fraction{0.25}}};
    REQUIRE(impossible.load(one, {}, 1) == LoadError::CapacityExceeded);
}

TEST_CASE("no authored deadlines means uninterrupted clock with real interval events", "[clock]") {
    World world({1, 4, 32});
    constexpr std::array controls{InitialControl{test::room, Fraction{0.25}}};
    REQUIRE(world.load(controls, {}, 9) == LoadError::None);
    REQUIRE(world.advance({60, 60}) == AdvanceResult{60, AdvanceStop::ReachedTarget});
    REQUIRE(world.view().next_decision_second == -1);
    REQUIRE(world.events().size() == 13);
    REQUIRE(world.events()[0].second == 5);
    REQUIRE(world.events()[0].type == EventType::ClimateBoundary);
    REQUIRE(world.events()[11].second == 60);
    REQUIRE(world.events()[11].type == EventType::ClimateBoundary);
    REQUIRE(world.events()[12].second == 60);
    REQUIRE(world.events()[12].type == EventType::CropBoundary);
}

TEST_CASE("authored decisions at 7 and 83 stop exactly and leave interval hooks intact", "[clock]") {
    World world({1, 4, 32});
    constexpr std::array controls{InitialControl{test::room, Fraction{0.25}}};
    constexpr std::array deadlines{DecisionDeadline{{0, 7}, test::room, 7},
                                   DecisionDeadline{{0, 83}, test::room, 83}};
    REQUIRE(world.load(controls, deadlines, 9) == LoadError::None);
    REQUIRE(world.advance({90, 90}) == AdvanceResult{7, AdvanceStop::DecisionRequired});
    REQUIRE(world.events()[0].type == EventType::ClimateBoundary);
    REQUIRE(world.events()[1].second == 7);
    REQUIRE(world.submit(test::resolve({0, 101}, 0, 7, {0, 7})).error == SubmitError::None);
    REQUIRE(world.advance({90, 90}) == AdvanceResult{83, AdvanceStop::DecisionRequired});
    REQUIRE(world.view().climate_boundaries == 16);
    REQUIRE(world.view().crop_boundaries == 1);
    REQUIRE(world.submit(test::resolve({0, 102}, 1, 83, {0, 83})).error == SubmitError::None);
    REQUIRE(world.advance({90, 7}) == AdvanceResult{90, AdvanceStop::ReachedTarget});
}

TEST_CASE("same-time decisions resolve in stable entity and decision ID order", "[clock]") {
    World world({2, 4, 10});
    constexpr std::array controls{InitialControl{{0, 2}, Fraction{0.3}}, InitialControl{{0, 1}, Fraction{0.2}}};
    constexpr std::array deadlines{DecisionDeadline{{0, 22}, {0, 2}, 7},
                                   DecisionDeadline{{0, 11}, {0, 1}, 7},
                                   DecisionDeadline{{0, 12}, {0, 1}, 7}};
    REQUIRE(world.load(controls, deadlines, 9) == LoadError::None);
    REQUIRE(world.advance({9, 9}) == AdvanceResult{7, AdvanceStop::DecisionRequired});
    REQUIRE(world.events()[1].entity == Id{0, 1});
    REQUIRE(world.events()[1].command_id == Id{0, 11});
    REQUIRE(world.events()[2].command_id == Id{0, 12});
    REQUIRE(world.events()[3].command_id == Id{0, 22});
    REQUIRE(world.submit(test::resolve({0, 101}, 0, 7, {0, 22})).error == SubmitError::InvalidDecision);
    REQUIRE(world.submit(test::resolve({0, 101}, 0, 7, {0, 11})).error == SubmitError::None);
    REQUIRE(world.advance({9, 9}) == AdvanceResult{7, AdvanceStop::DecisionRequired});
    REQUIRE(world.submit(test::resolve({0, 102}, 1, 7, {0, 12})).error == SubmitError::None);
    REQUIRE(world.submit(test::resolve({0, 103}, 2, 7, {0, 22})).error == SubmitError::None);
    REQUIRE(world.advance({9, 2}) == AdvanceResult{9, AdvanceStop::ReachedTarget});
}

TEST_CASE("deadline load rejects invalid time, duplicate ID, unknown entity and capacity without admission", "[clock]") {
    World world({1, 4, 4, 1});
    constexpr std::array controls{InitialControl{test::room, Fraction{0.2}}};
    constexpr std::array two{DecisionDeadline{{0, 1}, test::room, 7}, DecisionDeadline{{0, 2}, test::room, 8}};
    REQUIRE(world.load(controls, two, 1) == LoadError::CapacityExceeded);
    constexpr std::array invalid{DecisionDeadline{{0, 1}, test::room, 0}};
    REQUIRE(world.load(controls, invalid, 1) == LoadError::InvalidTime);
    constexpr std::array unknown{DecisionDeadline{{0, 1}, {0, 99}, 7}};
    REQUIRE(world.load(controls, unknown, 1) == LoadError::UnknownEntity);
    World duplicate({1, 4, 4});
    constexpr std::array repeated{DecisionDeadline{{0, 1}, test::room, 7}, DecisionDeadline{{0, 1}, test::room, 8}};
    REQUIRE(duplicate.load(controls, repeated, 1) == LoadError::DuplicateDecision);
    REQUIRE(world.view().controls.empty());
    REQUIRE(duplicate.view().controls.empty());
}
