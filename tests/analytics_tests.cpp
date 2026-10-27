#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <quantlab/analytics.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void check_vector(const std::vector<double>& actual,
                  const std::vector<double>& expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CAPTURE(index);
        CHECK(actual[index] == doctest::Approx(expected[index]).epsilon(1e-12));
    }
}

const double not_a_number = std::numeric_limits<double>::quiet_NaN();
const double infinity = std::numeric_limits<double>::infinity();

}  // namespace

TEST_CASE("simple returns use consecutive prices and preserve their order") {
    check_vector(quantlab::simple_returns({100.0, 110.0, 99.0, 99.0}),
                 {0.1, -0.1, 0.0});
    check_vector(quantlab::simple_returns({10.0, 15.0}), {0.5});
    check_vector(quantlab::simple_returns({42.0, 42.0, 42.0}), {0.0, 0.0});
}

TEST_CASE("simple returns are invariant under a common positive price scale") {
    check_vector(quantlab::simple_returns({100.0, 110.0, 88.0, 132.0}),
                 quantlab::simple_returns({1000.0, 1100.0, 880.0, 1320.0}));
}

TEST_CASE("simple returns reject insufficient or invalid prices") {
    CHECK_THROWS_AS((void)quantlab::simple_returns({}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({100.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({0.0, 100.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({100.0, 0.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({100.0, -1.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({not_a_number, 100.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({100.0, infinity}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::simple_returns({100.0, -infinity}), std::invalid_argument);
}

TEST_CASE("simple returns report overflow instead of returning infinity") {
    CHECK_THROWS_AS(
        (void)quantlab::simple_returns({std::numeric_limits<double>::denorm_min(),
                                 std::numeric_limits<double>::max()}),
        std::overflow_error);
}

TEST_CASE("moving average returns only complete windows") {
    check_vector(quantlab::moving_average({1.0, 2.0, 3.0, 4.0, 5.0}, 3),
                 {2.0, 3.0, 4.0});
    check_vector(quantlab::moving_average({-3.0, 1.0, 5.0, -1.0}, 2),
                 {-1.0, 3.0, 2.0});
}

TEST_CASE("moving average handles both legal window boundaries") {
    const std::vector<double> values{1.0, -2.0, 6.0, 3.0};
    check_vector(quantlab::moving_average(values, 1), values);
    check_vector(quantlab::moving_average(values, values.size()), {2.0});
    check_vector(quantlab::moving_average({7.0}, 1), {7.0});
    check_vector(quantlab::moving_average({0.0, 0.0, 0.0}, 2), {0.0, 0.0});
}

TEST_CASE("moving average preserves small changes on a large offset") {
    const auto actual = quantlab::moving_average(
        {1e12 + 1.0, 1e12 + 2.0, 1e12 + 3.0, 1e12 + 4.0}, 3);
    REQUIRE(actual.size() == 2);
    CHECK(actual[0] - 1e12 == doctest::Approx(2.0).epsilon(1e-12));
    CHECK(actual[1] - 1e12 == doctest::Approx(3.0).epsilon(1e-12));
}

TEST_CASE("moving average avoids overflow when the actual mean is finite") {
    const double largest = std::numeric_limits<double>::max();
    const auto actual = quantlab::moving_average({largest, largest, -largest}, 2);
    REQUIRE(actual.size() == 2);
    CHECK(actual[0] == largest);
    CHECK(actual[1] == 0.0);
}

TEST_CASE("moving average preserves a full window of the largest finite value") {
    const double largest = std::numeric_limits<double>::max();
    const auto actual = quantlab::moving_average({largest, largest, largest}, 3);
    REQUIRE(actual.size() == 1);
    CHECK(actual[0] == largest);
}

TEST_CASE("moving average recovers tiny values after a large outlier leaves") {
    const double largest = std::numeric_limits<double>::max();
    for (const double tiny : {1e-200, std::numeric_limits<double>::denorm_min()}) {
        CAPTURE(tiny);
        const auto actual = quantlab::moving_average({largest, tiny, tiny}, 2);
        REQUIRE(actual.size() == 2);
        CHECK(actual[0] == largest / 2.0);
        // Exact equality deliberately catches losing a representable subnormal.
        CHECK(actual[1] == tiny);
    }
}

TEST_CASE("moving average rejects invalid windows and nonfinite values") {
    CHECK_THROWS_AS((void)quantlab::moving_average({}, 1), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::moving_average({1.0, 2.0}, 0), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::moving_average({1.0, 2.0}, 3), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::moving_average({not_a_number, 1.0}, 1), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::moving_average({1.0, infinity}, 2), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::moving_average({1.0, -infinity}, 2), std::invalid_argument);
}

TEST_CASE("volatility uses the sample denominator n minus one") {
    const std::vector<double> returns{0.1, -0.1, 0.1, -0.1};
    const double expected = std::sqrt(0.04 / 3.0);
    CHECK(quantlab::sample_volatility(returns, 1.0) ==
          doctest::Approx(expected).epsilon(1e-12));
    CHECK(quantlab::sample_volatility({-0.1, 0.1}, 1.0) ==
          doctest::Approx(std::sqrt(0.02)).epsilon(1e-12));
}

TEST_CASE("volatility annualizes with the square root of periods") {
    const std::vector<double> returns{-0.02, 0.01, 0.03, -0.01, 0.005};
    const double daily = quantlab::sample_volatility(returns, 1.0);
    CHECK(quantlab::sample_volatility(returns) ==
          doctest::Approx(daily * std::sqrt(252.0)).epsilon(1e-12));
    CHECK(quantlab::sample_volatility(returns, 12.0) ==
          doctest::Approx(daily * std::sqrt(12.0)).epsilon(1e-12));
    CHECK(quantlab::sample_volatility({0.02, 0.02, 0.02}) == 0.0);
}

TEST_CASE("volatility remains accurate for a large common offset") {
    CHECK(quantlab::sample_volatility(
              {1e12 + 1.0, 1e12 + 2.0, 1e12 + 3.0, 1e12 + 4.0}, 1.0) ==
          doctest::Approx(std::sqrt(5.0 / 3.0)).epsilon(1e-10));
}

TEST_CASE("volatility scales extreme inputs before squaring") {
    const double largest = std::numeric_limits<double>::max();
    const double periods = std::numeric_limits<double>::min();
    // The unscaled standard deviation exceeds DBL_MAX, but this result fits.
    const double expected = (largest * std::sqrt(periods)) * std::sqrt(2.0);
    const double actual = quantlab::sample_volatility({-largest, largest}, periods);
    CHECK(std::isfinite(actual));
    CHECK(actual == doctest::Approx(expected).epsilon(1e-12));
}

TEST_CASE("volatility reports a nonrepresentable result as overflow") {
    const double largest = std::numeric_limits<double>::max();
    CHECK_THROWS_AS((void)quantlab::sample_volatility({-largest, largest}, 1.0),
                    std::overflow_error);
}

TEST_CASE("volatility rejects insufficient samples and invalid annualization") {
    CHECK_THROWS_AS((void)quantlab::sample_volatility({}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::sample_volatility({0.01}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::sample_volatility({0.01, not_a_number}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::sample_volatility({infinity, 0.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::sample_volatility({0.0, -infinity}), std::invalid_argument);
    for (const double periods : {0.0, -1.0, not_a_number, infinity}) {
        CAPTURE(periods);
        CHECK_THROWS_AS((void)quantlab::sample_volatility({0.01, 0.02}, periods),
                        std::invalid_argument);
    }
}

TEST_CASE("maximum drawdown measures the worst loss from a preceding peak") {
    CHECK(quantlab::maximum_drawdown({100.0, 120.0, 90.0, 150.0, 120.0}) ==
          doctest::Approx(0.25));
    CHECK(quantlab::maximum_drawdown({100.0, 80.0, 60.0}) ==
          doctest::Approx(0.4));
    CHECK(quantlab::maximum_drawdown({100.0, 110.0, 90.0, 120.0, 60.0, 130.0}) ==
          doctest::Approx(0.5));
}

TEST_CASE("maximum drawdown is zero without a decline") {
    CHECK(quantlab::maximum_drawdown({100.0}) == 0.0);
    CHECK(quantlab::maximum_drawdown({100.0, 110.0, 120.0}) == 0.0);
    CHECK(quantlab::maximum_drawdown({100.0, 100.0, 100.0}) == 0.0);
}

TEST_CASE("maximum drawdown is invariant under price scale and bounded") {
    const auto small = quantlab::maximum_drawdown({10.0, 12.0, 3.0, 11.0});
    const auto large = quantlab::maximum_drawdown({100.0, 120.0, 30.0, 110.0});
    CHECK(small == doctest::Approx(large).epsilon(1e-12));
    CHECK(small >= 0.0);
    CHECK(small <= 1.0);
    CHECK(small == doctest::Approx(0.75));
}

TEST_CASE("maximum drawdown rejects missing and invalid prices") {
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({0.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({100.0, -1.0}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({not_a_number}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({100.0, infinity}), std::invalid_argument);
    CHECK_THROWS_AS((void)quantlab::maximum_drawdown({100.0, -infinity}), std::invalid_argument);
}

TEST_CASE("analytics compose on one reproducible price series") {
    const std::vector<double> prices{100.0, 110.0, 99.0, 108.9};
    const auto returns = quantlab::simple_returns(prices);
    check_vector(returns, {0.1, -0.1, 0.1});
    check_vector(quantlab::moving_average(prices, 2), {105.0, 104.5, 103.95});
    CHECK(quantlab::sample_volatility(returns, 1.0) ==
          doctest::Approx(std::sqrt(0.04 / 3.0)).epsilon(1e-12));
    CHECK(quantlab::maximum_drawdown(prices) == doctest::Approx(0.1));
}
