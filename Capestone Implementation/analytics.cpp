#include "analytics.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iostream>

double AnalyticsEngine::calculate_sma(const std::vector<double>& prices, int period) {
    if (prices.empty() || period <= 0) return 0.0;
    int p = std::min(period, static_cast<int>(prices.size()));
    double sum = 0.0;
    for (int i = static_cast<int>(prices.size()) - p; i < static_cast<int>(prices.size()); ++i) {
        sum += prices[i];
    }
    return std::round((sum / p) * 100.0) / 100.0;
}

double AnalyticsEngine::calculate_ema(const std::vector<double>& prices, int period) {
    if (prices.empty() || period <= 0) return 0.0;
    double alpha = 2.0 / (period + 1.0);
    double ema = prices[0];
    for (size_t i = 1; i < prices.size(); ++i) {
        ema = (alpha * prices[i]) + ((1.0 - alpha) * ema);
    }
    return std::round(ema * 100.0) / 100.0;
}

double AnalyticsEngine::calculate_rsi(const std::vector<double>& prices, int period) {
    if (prices.size() <= static_cast<size_t>(period)) return 50.0;

    double gain_sum = 0.0;
    double loss_sum = 0.0;

    for (int i = 1; i <= period; ++i) {
        double diff = prices[i] - prices[i - 1];
        if (diff >= 0) gain_sum += diff;
        else loss_sum += std::abs(diff);
    }

    double avg_gain = gain_sum / period;
    double avg_loss = loss_sum / period;

    for (size_t i = period + 1; i < prices.size(); ++i) {
        double diff = prices[i] - prices[i - 1];
        if (diff >= 0) {
            avg_gain = (avg_gain * (period - 1) + diff) / period;
            avg_loss = (avg_loss * (period - 1)) / period;
        } else {
            avg_gain = (avg_gain * (period - 1)) / period;
            avg_loss = (avg_loss * (period - 1) + std::abs(diff)) / period;
        }
    }

    if (avg_loss < 1e-9) return 100.0;
    double rs = avg_gain / avg_loss;
    double rsi = 100.0 - (100.0 / (1.0 + rs));
    return std::round(rsi * 100.0) / 100.0;
}

void AnalyticsEngine::calculate_macd(const std::vector<double>& prices, double& out_macd, double& out_signal, double& out_hist) {
    if (prices.size() < 26) {
        out_macd = 0.0;
        out_signal = 0.0;
        out_hist = 0.0;
        return;
    }

    double alpha12 = 2.0 / 13.0;
    double alpha26 = 2.0 / 27.0;

    double ema12 = prices[0];
    double ema26 = prices[0];

    std::vector<double> macd_series;
    macd_series.reserve(prices.size());

    for (size_t i = 0; i < prices.size(); ++i) {
        if (i > 0) {
            ema12 = (alpha12 * prices[i]) + ((1.0 - alpha12) * ema12);
            ema26 = (alpha26 * prices[i]) + ((1.0 - alpha26) * ema26);
        }
        macd_series.push_back(ema12 - ema26);
    }

    double alpha9 = 2.0 / 10.0;
    double signal = macd_series[0];
    for (size_t i = 1; i < macd_series.size(); ++i) {
        signal = (alpha9 * macd_series[i]) + ((1.0 - alpha9) * signal);
    }

    out_macd = std::round(macd_series.back() * 100.0) / 100.0;
    out_signal = std::round(signal * 100.0) / 100.0;
    out_hist = std::round((out_macd - out_signal) * 100.0) / 100.0;
}

void AnalyticsEngine::calculate_bollinger_bands(
    const std::vector<double>& prices,
    int period,
    double num_std,
    double& out_upper,
    double& out_middle,
    double& out_lower
) {
    if (prices.empty()) {
        out_upper = out_middle = out_lower = 0.0;
        return;
    }

    int p = std::min(period, static_cast<int>(prices.size()));
    double sum = 0.0;
    int start_idx = static_cast<int>(prices.size()) - p;
    for (int i = start_idx; i < static_cast<int>(prices.size()); ++i) {
        sum += prices[i];
    }
    double middle = sum / p;

    double var_sum = 0.0;
    for (int i = start_idx; i < static_cast<int>(prices.size()); ++i) {
        var_sum += (prices[i] - middle) * (prices[i] - middle);
    }
    double std_dev = std::sqrt(var_sum / p);

    out_middle = std::round(middle * 100.0) / 100.0;
    out_upper = std::round((middle + (num_std * std_dev)) * 100.0) / 100.0;
    out_lower = std::round((middle - (num_std * std_dev)) * 100.0) / 100.0;
}

double AnalyticsEngine::calculate_volatility(const std::vector<double>& prices) {
    if (prices.size() < 2) return 0.0;
    std::vector<double> daily_returns;
    daily_returns.reserve(prices.size() - 1);
    double sum_ret = 0.0;

    for (size_t i = 1; i < prices.size(); ++i) {
        double r = (prices[i] - prices[i - 1]) / prices[i - 1];
        daily_returns.push_back(r);
        sum_ret += r;
    }

    double mean_ret = sum_ret / daily_returns.size();
    double var_sum = 0.0;
    for (double r : daily_returns) {
        var_sum += (r - mean_ret) * (r - mean_ret);
    }
    double daily_std = std::sqrt(var_sum / daily_returns.size());
    // Annualized volatility
    double ann_vol = daily_std * std::sqrt(252.0) * 100.0;
    return std::round(ann_vol * 100.0) / 100.0;
}

TechnicalIndicators AnalyticsEngine::compute_indicators(const std::vector<StockPrice>& history, const std::string& symbol) {
    TechnicalIndicators ti;
    ti.symbol = symbol;

    if (history.empty()) return ti;

    std::vector<double> close_prices;
    close_prices.reserve(history.size());
    double w52_h = history[0].high;
    double w52_l = history[0].low;

    for (const auto& sp : history) {
        close_prices.push_back(sp.close);
        if (sp.high > w52_h) w52_h = sp.high;
        if (sp.low < w52_l) w52_l = sp.low;
    }

    ti.current_price = close_prices.back();
    ti.daily_change_pct = history.back().change_percent;
    ti.daily_change = std::round((ti.current_price * (ti.daily_change_pct / 100.0)) * 100.0) / 100.0;
    ti.week52_high = w52_h;
    ti.week52_low = w52_l;

    ti.sma20 = calculate_sma(close_prices, 20);
    ti.sma50 = calculate_sma(close_prices, 50);
    ti.ema12 = calculate_ema(close_prices, 12);
    ti.ema26 = calculate_ema(close_prices, 26);
    ti.rsi14 = calculate_rsi(close_prices, 14);
    calculate_macd(close_prices, ti.macd, ti.macd_signal, ti.macd_hist);
    calculate_bollinger_bands(close_prices, 20, 2.0, ti.bb_upper, ti.bb_middle, ti.bb_lower);
    ti.volatility = calculate_volatility(close_prices);

    // Rule-based Decision Engine
    int bullish_score = 0;
    int bearish_score = 0;

    // Rule 1: Moving Average Trend Alignment
    if (ti.current_price > ti.sma20 && ti.sma20 > ti.sma50) {
        bullish_score += 2;
        ti.reasons.push_back("Price is above both 20-day SMA (" + std::to_string(ti.sma20).substr(0, 7) + ") and 50-day SMA, confirming Golden Trend alignment.");
    } else if (ti.current_price < ti.sma20 && ti.sma20 < ti.sma50) {
        bearish_score += 2;
        ti.reasons.push_back("Price is trading below both 20-day SMA (" + std::to_string(ti.sma20).substr(0, 7) + ") and 50-day SMA, indicating sustained downward trend.");
    } else {
        ti.reasons.push_back("Price oscillates around key moving averages, reflecting short-term consolidation.");
    }

    // Rule 2: RSI Evaluation
    if (ti.rsi14 < 30.0) {
        bullish_score += 2;
        ti.reasons.push_back("RSI(14) is at " + std::to_string(ti.rsi14).substr(0, 5) + " (Oversold condition < 30), signaling high probability of mean-reversion rebound.");
    } else if (ti.rsi14 > 70.0) {
        bearish_score += 2;
        ti.reasons.push_back("RSI(14) is at " + std::to_string(ti.rsi14).substr(0, 5) + " (Overbought territory > 70), indicating elevated risk of price correction.");
    } else if (ti.rsi14 >= 50.0) {
        bullish_score += 1;
        ti.reasons.push_back("RSI(14) is at " + std::to_string(ti.rsi14).substr(0, 5) + ", holding in healthy bullish expansion zone.");
    } else {
        bearish_score += 1;
        ti.reasons.push_back("RSI(14) is at " + std::to_string(ti.rsi14).substr(0, 5) + ", displaying mild bearish weakness.");
    }

    // Rule 3: MACD Momentum
    if (ti.macd_hist > 0) {
        bullish_score += 2;
        ti.reasons.push_back("MACD line (" + std::to_string(ti.macd).substr(0, 5) + ") is above Signal line with positive histogram (" + std::to_string(ti.macd_hist).substr(0, 5) + "), affirming bullish momentum.");
    } else {
        bearish_score += 2;
        ti.reasons.push_back("MACD line is below Signal line with negative histogram (" + std::to_string(ti.macd_hist).substr(0, 5) + "), pointing to bearish momentum.");
    }

    // Rule 4: Bollinger Bands Boundary test
    if (ti.current_price <= ti.bb_lower) {
        bullish_score += 1;
        ti.reasons.push_back("Price tested lower Bollinger Band (" + std::to_string(ti.bb_lower).substr(0, 7) + "), historically functioning as strong dynamic support.");
    } else if (ti.current_price >= ti.bb_upper) {
        bearish_score += 1;
        ti.reasons.push_back("Price touched upper Bollinger Band (" + std::to_string(ti.bb_upper).substr(0, 7) + "), reaching resistance ceiling.");
    }

    // Determine final signal & confidence
    if (bullish_score >= bearish_score + 2) {
        ti.signal = "BUY";
        ti.trend = "Upward (Bullish)";
        ti.confidence = std::clamp(65.0 + (bullish_score * 4.5), 65.0, 95.0);
    } else if (bearish_score >= bullish_score + 2) {
        ti.signal = "SELL";
        ti.trend = "Downward (Bearish)";
        ti.confidence = std::clamp(65.0 + (bearish_score * 4.5), 65.0, 95.0);
    } else {
        ti.signal = "HOLD";
        ti.trend = "Sideways / Neutral";
        ti.confidence = 70.0;
        ti.reasons.push_back("Mixed technical signals detected. Maintaining current market exposure recommended.");
    }

    return ti;
}
