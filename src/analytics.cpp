#include "quantlab/analytics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require_finite(double value, const char* description) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(description) + " must be finite");
    }
}

void require_positive_price(double price) {
    require_finite(price, "Price");
    if (price <= 0.0) {
        throw std::invalid_argument("Prices must be strictly positive");
    }
}

double checked_double(long double value, const char* description) {
    if (!std::isfinite(value) ||
        std::abs(value) > static_cast<long double>(std::numeric_limits<double>::max())) {
        throw std::overflow_error(std::string(description) + " exceeds finite double range");
    }
    return static_cast<double>(value);
}

// Neumaier compensation retains small contributions even after cancellation.
class CompensatedSum {
public:
    void add(long double value) {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const { return sum_ + correction_; }

private:
    long double sum_ = 0.0L;
    long double correction_ = 0.0L;
};

}  // namespace

namespace quantlab {

std::vector<double> simple_returns(const std::vector<double>& prices) {
    if (prices.size() < 2) {
        throw std::invalid_argument("Simple returns require at least two prices");
    }
    for (double price : prices) {
        require_positive_price(price);
    }

    std::vector<double> result;
    result.reserve(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); ++i) {
        const long double ratio = static_cast<long double>(prices[i]) / prices[i - 1];
        result.push_back(checked_double(ratio - 1.0L, "Simple return"));
    }
    return result;
}

std::vector<double> moving_average(const std::vector<double>& values, std::size_t window) {
    if (window == 0 || window > values.size()) {
        throw std::invalid_argument("Moving-average window must be in [1, values.size()]");
    }
    for (double value : values) {
        require_finite(value, "Observation");
    }

    const long double divisor = static_cast<long double>(window);
    const long double maximum = std::numeric_limits<double>::max();
    const long double small_limit = (maximum / 4.0L) / divisor;
    CompensatedSum small_sum;
    CompensatedSum large_mean;
    const auto add = [&](long double value) {
        if (std::abs(value) <= small_limit) {
            // Keep small observations unscaled so subnormal values survive.
            small_sum.add(value);
        } else {
            // The extra factor of four leaves rounding headroom even when
            // long double has double's range and every observation is DBL_MAX.
            large_mean.add((value / divisor) / 4.0L);
        }
    };
    const auto current_mean = [&]() {
        const long double large_part =
            std::clamp(large_mean.value(), -maximum / 4.0L, maximum / 4.0L) * 4.0L;
        const long double mean = large_part + small_sum.value() / divisor;
        // An average of finite doubles lies within their representable range.
        // Clamp the last rounding step at this mathematical bound.
        return checked_double(std::clamp(mean, -maximum, maximum), "Moving average");
    };
    for (std::size_t i = 0; i < window; ++i) {
        add(values[i]);
    }

    std::vector<double> result;
    result.reserve(values.size() - window + 1);
    result.push_back(current_mean());
    for (std::size_t i = window; i < values.size(); ++i) {
        add(-static_cast<long double>(values[i - window]));
        add(values[i]);
        result.push_back(current_mean());
    }
    return result;
}

double sample_volatility(const std::vector<double>& returns, double periods_per_year) {
    if (returns.size() < 2) {
        throw std::invalid_argument("Sample volatility requires at least two returns");
    }
    require_finite(periods_per_year, "Periods per year");
    if (periods_per_year <= 0.0) {
        throw std::invalid_argument("Periods per year must be strictly positive");
    }
    for (double value : returns) {
        require_finite(value, "Return");
    }

    // Center first to preserve small variations around a large common offset.
    // Normalize before squaring so the algorithm also works where long double
    // has the same range as double (for example, Microsoft Visual C++).
    long double origin = returns.front();
    long double scale = 0.0L;
    for (double value : returns) {
        const long double difference = static_cast<long double>(value) - origin;
        if (!std::isfinite(difference)) {
            origin = 0.0L;
            break;
        }
        scale = std::max(scale, std::abs(difference));
    }
    if (origin == 0.0L) {
        for (double value : returns) {
            scale = std::max(scale, std::abs(static_cast<long double>(value)));
        }
    }
    if (scale == 0.0L) {
        return 0.0;
    }

    std::size_t count = 0;
    long double mean = 0.0L;
    long double squared_deviations = 0.0L;
    for (double value : returns) {
        const long double normalized = (static_cast<long double>(value) - origin) / scale;
        ++count;
        const long double delta = normalized - mean;
        mean += delta / static_cast<long double>(count);
        const long double delta_after = normalized - mean;
        squared_deviations += delta * delta_after;
    }

    const long double variance = squared_deviations / static_cast<long double>(count - 1);
    // Taking the roots separately avoids overflowing variance * periods_per_year.
    const long double volatility = (std::sqrt(std::max(variance, 0.0L)) *
                                    std::sqrt(static_cast<long double>(periods_per_year))) * scale;
    return checked_double(volatility, "Sample volatility");
}

double maximum_drawdown(const std::vector<double>& prices) {
    if (prices.empty()) {
        throw std::invalid_argument("Maximum drawdown requires at least one price");
    }

    long double peak = 0.0L;
    long double drawdown = 0.0L;
    for (double price : prices) {
        require_positive_price(price);
        peak = std::max(peak, static_cast<long double>(price));
        drawdown = std::max(drawdown, (peak - price) / peak);
    }
    return static_cast<double>(drawdown);
}

}  // namespace quantlab
