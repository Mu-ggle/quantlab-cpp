#include "quantlab/analytics.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Usage: quantlab_demo [--help]\n"
                         "Analyze a built-in six-price example with QuantLab.\n"
                         "Returns, volatility and drawdown are displayed as percentages.\n";
            return 0;
        }
        std::cerr << "Unknown argument. Usage: quantlab_demo [--help]\n";
        return 2;
    }

    try {
        const std::vector<double> prices{100.0, 102.0, 101.0, 105.0, 103.0, 108.0};
        const auto returns = quantlab::simple_returns(prices);
        const auto averages = quantlab::moving_average(prices, 3);

        std::cout << std::fixed << std::setprecision(6)
                  << "QuantLab: financial time-series analytics\n"
                  << "Prices:";
        for (double price : prices) {
            std::cout << ' ' << price;
        }
        std::cout << "\nSimple returns (%):";
        for (double value : returns) {
            std::cout << ' ' << value * 100.0;
        }
        std::cout << "\nMoving average (window=3):";
        for (double average : averages) {
            std::cout << ' ' << average;
        }
        std::cout << "\nAnnualized sample volatility (252 periods, %): "
                  << quantlab::sample_volatility(returns) * 100.0
                  << "\nMaximum drawdown (%): "
                  << quantlab::maximum_drawdown(prices) * 100.0 << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Analysis failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
