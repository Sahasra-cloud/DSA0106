#include "stock_data.hpp"
#include <iostream>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

std::vector<Stock> StockDataManager::get_default_stocks() {
    return {
        {1, "TCS", "Tata Consultancy Services Ltd", "NSE (India)", "INR", 3940.50, 24.30, 0.62},
        {2, "INFY", "Infosys Limited", "NSE (India)", "INR", 1685.20, -12.40, -0.73},
        {3, "RELIANCE", "Reliance Industries Limited", "NSE (India)", "INR", 2860.80, 18.50, 0.65},
        {4, "AAPL", "Apple Inc.", "NASDAQ (US)", "USD", 228.45, 1.85, 0.82},
        {5, "GOOGL", "Alphabet Inc.", "NASDAQ (US)", "USD", 176.80, -0.95, -0.53},
        {6, "MSFT", "Microsoft Corporation", "NASDAQ (US)", "USD", 432.10, 3.20, 0.75}
    };
}

std::vector<StockPrice> StockDataManager::generate_realistic_history(const std::string& symbol, int trading_days) {
    std::vector<StockPrice> prices;
    prices.reserve(trading_days);

    double base_price = 100.0;
    double daily_volatility = 0.015;
    long long base_volume = 1000000;
    double annual_drift = 0.08; // 8% drift

    if (symbol == "TCS") {
        base_price = 3750.0;
        daily_volatility = 0.012;
        base_volume = 1800000;
    } else if (symbol == "INFY") {
        base_price = 1550.0;
        daily_volatility = 0.016;
        base_volume = 4500000;
    } else if (symbol == "RELIANCE") {
        base_price = 2650.0;
        daily_volatility = 0.014;
        base_volume = 5200000;
    } else if (symbol == "AAPL") {
        base_price = 195.0;
        daily_volatility = 0.013;
        base_volume = 48000000;
    } else if (symbol == "GOOGL") {
        base_price = 152.0;
        daily_volatility = 0.017;
        base_volume = 22000000;
    } else if (symbol == "MSFT") {
        base_price = 398.0;
        daily_volatility = 0.014;
        base_volume = 19000000;
    }

    // Anchor end date near current local time: 2026-11-20
    struct tm end_tm = {};
    end_tm.tm_year = 2026 - 1900;
    end_tm.tm_mon = 10; // November (0-based)
    end_tm.tm_mday = 20;
    end_tm.tm_hour = 12;
    time_t end_time = mktime(&end_tm);

    // Compute dates backwards for trading_days (excluding Saturdays & Sundays)
    std::vector<std::string> trading_dates;
    trading_dates.reserve(trading_days);
    time_t cur = end_time;

    while (static_cast<int>(trading_dates.size()) < trading_days) {
        struct tm* tinfo = localtime(&cur);
        if (tinfo->tm_wday != 0 && tinfo->tm_wday != 6) { // Not Sunday (0) and not Saturday (6)
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d", tinfo);
            trading_dates.push_back(buf);
        }
        cur -= 86400; // Subtract 1 day
    }

    // Reverse to chronological order (oldest to newest)
    std::reverse(trading_dates.begin(), trading_dates.end());

    // Deterministic pseudo-random seed based on symbol
    unsigned int seed = 42;
    for (char c : symbol) seed = seed * 31 + c;

    auto next_rand = [&seed]() -> double {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        return static_cast<double>(seed) / 2147483647.0;
    };

    auto next_gaussian = [&next_rand]() -> double {
        double u1 = next_rand();
        double u2 = next_rand();
        if (u1 <= 1e-15) u1 = 1e-15;
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 3.141592653589793 * u2);
    };

    double current_close = base_price;
    double dt = 1.0 / 252.0;

    for (int i = 0; i < trading_days; ++i) {
        StockPrice sp;
        sp.symbol = symbol;
        sp.date = trading_dates[i];

        // Geometric Brownian Motion step
        double z = next_gaussian();
        // Add periodic cyclical market oscillations (macro trend)
        double cycle = 0.003 * std::sin(2.0 * 3.141592653589793 * i / 45.0);
        double return_step = (annual_drift * dt) + (daily_volatility * std::sqrt(dt) * z) + cycle;

        double open_p = current_close * (1.0 + (next_gaussian() * 0.003));
        double close_p = current_close * std::exp(return_step);

        double intra_high = std::max(open_p, close_p) * (1.0 + std::abs(next_gaussian()) * 0.006 + 0.001);
        double intra_low = std::min(open_p, close_p) * (1.0 - std::abs(next_gaussian()) * 0.006 - 0.001);

        double change_pct = ((close_p - current_close) / current_close) * 100.0;
        if (i == 0) change_pct = ((close_p - open_p) / open_p) * 100.0;

        // Volume calculation correlated with price volatility
        double vol_factor = 1.0 + std::abs(change_pct) * 0.2 + (next_rand() * 0.4 - 0.2);
        long long vol = static_cast<long long>(base_volume * vol_factor);

        sp.open = std::round(open_p * 100.0) / 100.0;
        sp.high = std::round(intra_high * 100.0) / 100.0;
        sp.low = std::round(intra_low * 100.0) / 100.0;
        sp.close = std::round(close_p * 100.0) / 100.0;
        sp.volume = vol;
        sp.change_percent = std::round(change_pct * 100.0) / 100.0;

        current_close = close_p;
        prices.push_back(sp);
    }

    return prices;
}

void StockDataManager::populate_database_if_empty(StockDatabase& db) {
    int stock_count = db.count_total_stocks();
    if (stock_count == 0) {
        std::cout << "[StockDataManager] Initializing database with popular default stocks..." << std::endl;
        auto default_stocks = get_default_stocks();
        for (const auto& s : default_stocks) {
            db.insert_stock(s);
        }
    }

    int price_count = db.count_total_prices();
    if (price_count == 0) {
        std::cout << "[StockDataManager] Generating 180 days of realistic historical market data for all stocks..." << std::endl;
        auto default_stocks = get_default_stocks();
        for (const auto& s : default_stocks) {
            auto history = generate_realistic_history(s.symbol, 180);
            db.batch_insert_prices(history);
            std::cout << "  -> Seeded " << history.size() << " daily records for " << s.symbol << std::endl;
        }
    }
}
