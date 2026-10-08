#include "fixtures/clock_world.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <optional>
#include <utility>

using namespace canopy;

TEST_CASE("repeated control delivery returns original receipt and conflicts on changed payload", "[commands]") {
    auto world = test::make_world();
    const auto command = test::control();
    const auto first = world.submit(command);
    REQUIRE(first.error == SubmitError::None);
    REQUIRE(first.receipt.revision == 1);
    REQUIRE(world.submit(command) == first);
    REQUIRE(world.view().revision == 1);
    REQUIRE(world.events().size() == 1);
    REQUIRE(world.submit(test::control(test::first_command, 0.5)).error == SubmitError::Conflict);
    REQUIRE(world.view().revision == 1);
    REQUIRE(world.view().controls[0].requested.value == 0.75);
    REQUIRE(world.advance({3, 3}).stop == AdvanceStop::ReachedTarget);
    REQUIRE(world.submit(command) == first);
    REQUIRE(world.submit(test::control(test::first_command, 0.75, 999, 3)) == first);
    auto changed_actor = test::control(test::first_command, 0.75, 999, 3);
    changed_actor.header.actor_id = {0, 77};
    REQUIRE(world.submit(changed_actor).error == SubmitError::Conflict);
    REQUIRE(world.submit(test::control(test::first_command, 0.5)).error == SubmitError::Conflict);
    REQUIRE(world.view().revision == 1);
}

TEST_CASE("stale revisions, invalid times, IDs and fractions do not mutate authority", "[commands]") {
    auto world = test::make_world();
    REQUIRE(world.submit(test::control({0, 12}, 0.5, 1)).error == SubmitError::StaleRevision);
    REQUIRE(world.submit(test::control({0, 12}, 0.5, 0, 1)).error == SubmitError::InvalidTime);
    REQUIRE(world.submit(test::control({0, 12}, -0.01)).error == SubmitError::InvalidValue);
    REQUIRE(world.submit(test::control({0, 12}, std::numeric_limits<double>::quiet_NaN())).error == SubmitError::InvalidValue);
    REQUIRE(world.submit(test::control({0, 12}, 0.5, 0, 0, {0, 44})).error == SubmitError::UnknownEntity);
    REQUIRE(world.submit(test::control({0, 0})).error == SubmitError::InvalidId);
    REQUIRE(world.view().revision == 0);
    REQUIRE(world.events().empty());
}

TEST_CASE("receipt and event admission rejects atomically without losing recorded IDs", "[commands]") {
    auto world = test::make_world({1, 1, 1});
    const auto first = test::control();
    REQUIRE(world.submit(first).error == SubmitError::None);
    REQUIRE(world.submit(first).receipt.revision == 1);
    REQUIRE(world.submit(test::control({0, 2}, 0.5, 1)).error == SubmitError::CapacityExceeded);
    REQUIRE(world.view().revision == 1);
    REQUIRE(world.view().controls[0].requested.value == 0.75);
    REQUIRE(world.events().size() == 1);

    auto event_limited = test::make_world({1, 2, 1});
    REQUIRE(event_limited.submit(first).error == SubmitError::None);
    REQUIRE(event_limited.submit(test::control({0, 2}, 0.5, 1)).error == SubmitError::CapacityExceeded);
    REQUIRE(event_limited.view().revision == 1);
    REQUIRE(event_limited.receipts().size() == 1);
}

TEST_CASE("decision resolution is typed, idempotent and rejects stale or wrong boundary", "[commands]") {
    auto world = test::make_world();
    REQUIRE(world.submit(test::resolve({0, 2}, 0, 0)).error == SubmitError::InvalidDecision);
    REQUIRE(world.advance({60, 60}).stop == AdvanceStop::DecisionRequired);
    auto command = test::resolve({0, 2}, 0, 59);
    REQUIRE(world.submit(command).error == SubmitError::InvalidTime);
    command = test::resolve({0, 2}, 0, 60);
    const auto receipt = world.submit(command);
    REQUIRE(receipt.error == SubmitError::None);
    REQUIRE(world.submit(command) == receipt);
    auto replay = command;
    replay.header.issued_at = 61;
    replay.header.expected_revision = 999;
    REQUIRE(world.submit(replay) == receipt);
    REQUIRE(world.submit(test::resolve({0, 3}, 0, 60)).error == SubmitError::StaleRevision);
    REQUIRE(world.view().revision == 1);
    REQUIRE(world.advance({61, 1}).stop == AdvanceStop::ReachedTarget);
}

TEST_CASE("colliding receipt IDs remain independent and rejected IDs remain available", "[commands]") {
    auto world = test::make_world();
    const auto one = test::control({0, 1}, 0.4);
    const auto two = test::control({1, 0}, 0.6, 1);
    REQUIRE(world.submit(test::control({0, 3}, -1.0)).error == SubmitError::InvalidValue);
    const auto first = world.submit(one);
    const auto second = world.submit(two);
    REQUIRE(first.receipt.revision == 1);
    REQUIRE(second.receipt.revision == 2);
    REQUIRE(world.submit(test::control({0, 3}, 0.5, 2)).receipt.revision == 3);
    REQUIRE(world.submit(test::control({0, 1}, 0.4, 900, -3)) == first);
    REQUIRE(world.submit(test::control({1, 0}, 0.6, 900, -3)) == second);
    REQUIRE(world.submit(test::control({1, 0}, 0.9)).error == SubmitError::Conflict);
    REQUIRE(world.receipts().size() == 3);
}

TEST_CASE("moving authority preserves receipts and leaves a reusable unloaded source", "[commands]") {
    auto source = test::make_world();
    const auto command = test::control();
    const auto receipt = source.submit(command);
    REQUIRE(receipt.error == SubmitError::None);
    REQUIRE(source.advance({60, 60}).stop == AdvanceStop::DecisionRequired);
    const auto sample = source.sample(test::room, 3, 17);
    std::optional<World> destination;
    SECTION("move construction") {
        destination.emplace(std::move(source));
    }
    SECTION("move assignment replaces an already loaded authority") {
        destination.emplace(test::make_world());
        REQUIRE(destination->submit(test::control({0, 99}, 0.4)).error == SubmitError::None);
        *destination = std::move(source);
    }
    REQUIRE(source.advance({61, 61}).stop == AdvanceStop::InvalidModel);
    REQUIRE(source.submit(command).error == SubmitError::InvalidModel);
    REQUIRE(source.view().second == 0);
    REQUIRE(source.view().revision == 0);
    REQUIRE(source.view().controls.empty());
    REQUIRE(source.events().empty());
    REQUIRE(source.receipts().empty());
    REQUIRE(destination->submit(command) == receipt);
    REQUIRE(destination->view().second == 60);
    REQUIRE(destination->view().controls[0].applied == Fraction{0.75});
    REQUIRE(destination->view().climate_boundaries == 12);
    REQUIRE(destination->view().crop_boundaries == 1);
    REQUIRE(destination->sample(test::room, 3, 17) == sample);
    REQUIRE(destination->submit(test::resolve({0, 2}, 1, 60)).error == SubmitError::None);
    REQUIRE(destination->advance({120, 60}) == AdvanceResult{120, AdvanceStop::DecisionRequired});
    const std::array controls{InitialControl{test::room, Fraction{0.1}}};
    const std::array deadlines{DecisionDeadline{{0, 7}, test::room, 7}};
    REQUIRE(source.load(controls, deadlines, 99) == LoadError::None);
    REQUIRE(source.submit(command).receipt.revision == 1);
    REQUIRE(source.advance({8, 8}) == AdvanceResult{7, AdvanceStop::DecisionRequired});
    REQUIRE(source.view().controls[0].applied == Fraction{0.75});
    REQUIRE(source.view().climate_boundaries == 1);
    REQUIRE(source.view().crop_boundaries == 0);
}
