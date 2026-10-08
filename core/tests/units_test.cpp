#include <canopy/units.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <cmath>
#include <limits>

TEST_CASE("floor area converts square feet to square metres", "[units]") {
    REQUIRE(canopy::square_feet_to_square_metres(60'000.0) == Catch::Approx(5'574.1824));
    REQUIRE(canopy::square_feet_to_square_metres(1.0) == Catch::Approx(0.09290304));
}

TEST_CASE("zero and signed area preserve the conversion's linear domain", "[units]") {
    REQUIRE(canopy::square_feet_to_square_metres(0.0) == 0.0);
    REQUIRE(std::signbit(canopy::square_feet_to_square_metres(-0.0)));
    REQUIRE(canopy::square_feet_to_square_metres(-100.0) == Catch::Approx(-9.290304));
}

TEST_CASE("converted areas preserve ordering and additivity within rounding", "[units]") {
    for (double area : {0.25, 1.0, 50.0, 20'000.0, 1'000'000.0}) {
        const double one_floor = canopy::square_feet_to_square_metres(area);
        const double two_floors = canopy::square_feet_to_square_metres(area * 2.0);
        REQUIRE(one_floor > 0.0);
        REQUIRE(two_floors > one_floor);
        REQUIRE(two_floors == Catch::Approx(one_floor + one_floor));
    }
}

TEST_CASE("non-finite IEEE inputs remain non-finite", "[units]") {
    REQUIRE(std::isnan(canopy::square_feet_to_square_metres(std::numeric_limits<double>::quiet_NaN())));
    REQUIRE(canopy::square_feet_to_square_metres(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    REQUIRE(canopy::square_feet_to_square_metres(-std::numeric_limits<double>::infinity()) == -std::numeric_limits<double>::infinity());
}
