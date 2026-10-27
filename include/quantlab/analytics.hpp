#ifndef QUANTLAB_ANALYTICS_HPP
#define QUANTLAB_ANALYTICS_HPP

#include <cstddef>
#include <vector>

/** @file analytics.hpp
 * @brief Validated, linear-time analytics for financial time series.
 */

/** @namespace quantlab
 * @brief Small C++17 library for financial time-series analytics.
 *
 * Functions never modify their inputs. Returns, volatility and drawdown use
 * fractions: 0.02 means 2 percent. No function assumes a currency or downloads
 * market data.
 */
namespace quantlab {

/**
 * @brief Calculate consecutive simple returns, p[i] / p[i-1] - 1.
 * @param prices At least two finite, strictly positive prices in time order.
 * @return A vector with prices.size() - 1 returns, in the same time order.
 * @throws std::invalid_argument If there are fewer than two prices or a price
 * is nonfinite or nonpositive.
 * @throws std::overflow_error If a return cannot be represented as finite double.
 * @par Complexity
 * O(n) time and O(n) output storage.
 */
[[nodiscard]] std::vector<double> simple_returns(const std::vector<double>& prices);

/**
 * @brief Calculate the trailing simple moving average for complete windows.
 * @param values Finite observations in time order; negative values are allowed.
 * @param window Number of observations per window, from 1 to values.size().
 * @return values.size() - window + 1 averages. Result j averages values in
 * [j, j + window); partial windows are omitted.
 * @throws std::invalid_argument If a value is nonfinite or window is zero or
 * larger than the number of observations.
 * @throws std::overflow_error If a result cannot be represented as finite double.
 * @par Complexity
 * O(n) time, O(1) working storage and O(n) output storage. Compensated long-double
 * accumulation reduces rounding error in the rolling updates. Separate bounded
 * sums for small and large observations avoid overflow while retaining tiny
 * observations after larger values leave the window.
 */
[[nodiscard]] std::vector<double> moving_average(
    const std::vector<double>& values, std::size_t window);

/**
 * @brief Calculate annualized sample volatility with Welford's algorithm.
 *
 * Centering and scaling preserve small variations around a large common offset
 * and avoid overflow when accumulating squared deviations.
 * @param returns At least two finite returns, expressed as fractions.
 * @param periods_per_year Finite, strictly positive annualization factor.
 * The default of 252 represents trading-day observations; 1 disables scaling.
 * @return sqrt(sample variance) * sqrt(periods_per_year). The sample variance
 * uses the unbiased denominator returns.size() - 1. Constant inputs return zero.
 * @throws std::invalid_argument If the input contains fewer than two returns,
 * a return is nonfinite, or periods_per_year is nonfinite or nonpositive.
 * @throws std::overflow_error If volatility cannot be represented as finite double.
 * @par Complexity
 * O(n) time and O(1) working storage.
 */
[[nodiscard]] double sample_volatility(
    const std::vector<double>& returns, double periods_per_year = 252.0);

/**
 * @brief Find the largest peak-to-trough decline in a price series.
 * @param prices At least one finite, strictly positive price in time order.
 * @return The largest (running_peak - price) / running_peak, as a nonnegative
 * fraction in [0, 1]. One price or a nondecreasing series returns zero.
 * @throws std::invalid_argument If prices is empty or a price is nonfinite or
 * nonpositive.
 * @par Complexity
 * O(n) time and O(1) working storage.
 */
[[nodiscard]] double maximum_drawdown(const std::vector<double>& prices);

}  // namespace quantlab

#endif  // QUANTLAB_ANALYTICS_HPP
