#include "preprocessing.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

void PreprocessingEngine::compute_daily_returns(std::vector<StockPrice>& dataset) {
    if (dataset.empty()) return;
    for (size_t i = 0; i < dataset.size(); ++i) {
        if (i == 0) {
            if (dataset[i].open > 0) {
                dataset[i].change_percent = ((dataset[i].close - dataset[i].open) / dataset[i].open) * 100.0;
            }
        } else {
            double prev_close = dataset[i - 1].close;
            if (prev_close > 0) {
                dataset[i].change_percent = ((dataset[i].close - prev_close) / prev_close) * 100.0;
            }
        }
    }
}

std::vector<double> PreprocessingEngine::min_max_normalize(const std::vector<double>& values, double min_v, double max_v) {
    std::vector<double> normalized;
    normalized.reserve(values.size());
    double range = max_v - min_v;
    if (std::abs(range) < 1e-9) range = 1.0;

    for (double val : values) {
        double norm = (val - min_v) / range;
        normalized.push_back(std::clamp(norm, 0.0, 1.0));
    }
    return normalized;
}

std::vector<double> PreprocessingEngine::z_score_standardize(const std::vector<double>& values, double mean_v, double std_dev) {
    std::vector<double> standardized;
    standardized.reserve(values.size());
    if (std::abs(std_dev) < 1e-9) std_dev = 1.0;

    for (double val : values) {
        standardized.push_back((val - mean_v) / std_dev);
    }
    return standardized;
}

PreprocessingSummary PreprocessingEngine::clean_and_normalize(std::vector<StockPrice>& dataset, const std::string& symbol) {
    PreprocessingSummary summary;
    summary.symbol = symbol;
    summary.raw_records_count = static_cast<int>(dataset.size());

    if (dataset.empty()) {
        return summary;
    }

    // 1. Sort chronologically by date
    std::sort(dataset.begin(), dataset.end(), [](const StockPrice& a, const StockPrice& b) {
        return a.date < b.date;
    });

    // 2. Deduplicate records with identical dates
    std::vector<StockPrice> deduped;
    deduped.reserve(dataset.size());
    for (const auto& row : dataset) {
        if (!deduped.empty() && deduped.back().date == row.date) {
            summary.duplicates_removed++;
            // Update to latest valid
            if (row.close > 0) deduped.back() = row;
        } else {
            deduped.push_back(row);
        }
    }
    dataset = std::move(deduped);

    // 3. Handle missing or zero/corrupted values
    for (size_t i = 0; i < dataset.size(); ++i) {
        bool corrupted = false;
        if (dataset[i].close <= 0.0) {
            corrupted = true;
            summary.missing_values_handled++;
            if (i > 0) {
                dataset[i].close = dataset[i - 1].close;
            } else if (i + 1 < dataset.size() && dataset[i + 1].close > 0.0) {
                dataset[i].close = dataset[i + 1].close;
            } else {
                dataset[i].close = 100.0;
            }
        }
        if (dataset[i].open <= 0.0) {
            dataset[i].open = dataset[i].close;
            corrupted = true;
        }
        if (dataset[i].high < std::max(dataset[i].open, dataset[i].close)) {
            dataset[i].high = std::max(dataset[i].open, dataset[i].close) * 1.002;
        }
        if (dataset[i].low > std::min(dataset[i].open, dataset[i].close)) {
            dataset[i].low = std::min(dataset[i].open, dataset[i].close) * 0.998;
        }
        if (dataset[i].volume <= 0) {
            dataset[i].volume = (i > 0) ? dataset[i - 1].volume : 500000;
        }
    }

    // 4. Re-calculate daily returns
    compute_daily_returns(dataset);
    summary.cleaned_records_count = static_cast<int>(dataset.size());

    // 5. Compute descriptive statistics on Close prices
    std::vector<double> close_prices;
    close_prices.reserve(dataset.size());
    double sum = 0.0;
    double min_p = dataset[0].close;
    double max_p = dataset[0].close;

    for (const auto& sp : dataset) {
        close_prices.push_back(sp.close);
        sum += sp.close;
        if (sp.close < min_p) min_p = sp.close;
        if (sp.close > max_p) max_p = sp.close;
    }

    double mean_p = sum / dataset.size();
    double variance_sum = 0.0;
    for (double p : close_prices) {
        variance_sum += (p - mean_p) * (p - mean_p);
    }
    double std_dev = std::sqrt(variance_sum / dataset.size());

    summary.min_price = min_p;
    summary.max_price = max_p;
    summary.mean_price = mean_p;
    summary.std_dev_price = std_dev;

    // 6. Outlier detection using 3-sigma rule
    for (double p : close_prices) {
        if (std::abs(p - mean_p) > 3.0 * std_dev) {
            summary.outliers_detected++;
        }
    }

    // 7. Samples of normalized & standardized data (last 30 days)
    int sample_size = std::min(30, static_cast<int>(dataset.size()));
    int start_idx = static_cast<int>(dataset.size()) - sample_size;

    std::vector<double> sample_raw;
    for (int i = start_idx; i < static_cast<int>(dataset.size()); ++i) {
        sample_raw.push_back(dataset[i].close);
        summary.dates_sample.push_back(dataset[i].date);
    }

    summary.normalized_sample = min_max_normalize(sample_raw, min_p, max_p);
    summary.standardized_sample = z_score_standardize(sample_raw, mean_p, std_dev);

    return summary;
}
