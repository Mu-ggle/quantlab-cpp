#include <benchmark/benchmark.h>

#include <quantlab/analytics.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

// Deterministic synthetic data: input generation is outside the measured loop.
std::vector<double> make_prices(std::size_t size) {
    std::vector<double> prices;
    prices.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
        const double time = static_cast<double>(index);
        prices.push_back(100.0 * std::exp(0.00001 * time +
                                        0.03 * std::sin(0.013 * time)));
    }
    return prices;
}

std::vector<double> make_returns(std::size_t size) {
    std::vector<double> returns;
    returns.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
        const double time = static_cast<double>(index);
        returns.push_back(0.0002 + 0.01 * std::sin(0.017 * time) +
                          0.003 * std::cos(0.113 * time));
    }
    return returns;
}

void BM_SimpleReturns(benchmark::State& state) {
    const auto prices = make_prices(static_cast<std::size_t>(state.range(0)));
    for (auto iteration : state) {
        (void)iteration;
        const auto result = quantlab::simple_returns(prices);
        benchmark::DoNotOptimize(result.data());
        benchmark::DoNotOptimize(result.size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(prices.size()));
}

void BM_MovingAverage(benchmark::State& state) {
    const auto prices = make_prices(static_cast<std::size_t>(state.range(0)));
    constexpr std::size_t window = 20;
    for (auto iteration : state) {
        (void)iteration;
        const auto result = quantlab::moving_average(prices, window);
        benchmark::DoNotOptimize(result.data());
        benchmark::DoNotOptimize(result.size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(prices.size()));
}

void BM_SampleVolatility(benchmark::State& state) {
    const auto returns = make_returns(static_cast<std::size_t>(state.range(0)));
    for (auto iteration : state) {
        (void)iteration;
        double result = quantlab::sample_volatility(returns);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(returns.size()));
}

void BM_MaximumDrawdown(benchmark::State& state) {
    const auto prices = make_prices(static_cast<std::size_t>(state.range(0)));
    for (auto iteration : state) {
        (void)iteration;
        double result = quantlab::maximum_drawdown(prices);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(prices.size()));
}

}  // namespace

// Three sizes expose scaling behavior without making every CI push expensive.
BENCHMARK(BM_SimpleReturns)->Arg(256)->Arg(4096)->Arg(65536)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_MovingAverage)->Arg(256)->Arg(4096)->Arg(65536)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SampleVolatility)->Arg(256)->Arg(4096)->Arg(65536)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_MaximumDrawdown)->Arg(256)->Arg(4096)->Arg(65536)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
