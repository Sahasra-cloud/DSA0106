#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <filesystem>
#include "http_server.hpp"
#include "database.hpp"
#include "stock_data.hpp"
#include "preprocessing.hpp"
#include "prediction.hpp"
#include "analytics.hpp"
#include "json.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n";
    std::cout << " C++ Analytical Framework for Stock Price Forecasting\n";
    std::cout << " Backend Architecture: Pure C++17 with SQLite3 & REST\n";
    std::cout << "========================================================\n" << std::endl;

    // Determine paths with bulletproof resolution
    std::string db_path = "database/stocks.db";
    std::string frontend_dir = "frontend";

    if (fs::exists("../frontend")) {
        frontend_dir = "../frontend";
    } else if (fs::exists("frontend")) {
        frontend_dir = "frontend";
    } else if (fs::exists("index.html") || fs::exists("dashboard.html")) {
        frontend_dir = ".";
    }

    if (fs::exists("../database")) {
        db_path = "../database/stocks.db";
    } else if (fs::exists("database")) {
        db_path = "database/stocks.db";
    } else {
        db_path = "stocks.db";
    }

    // Ensure database directory exists
    try {
        fs::path p(db_path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    } catch (...) {}

    auto start_time = std::chrono::steady_clock::now();

    // Initialize Database
    auto db = std::make_shared<StockDatabase>();
    if (!db->init(db_path)) {
        std::cerr << "CRITICAL: Failed to initialize SQLite database at " << db_path << std::endl;
        return 1;
    }
    std::cout << "[Database] SQLite initialized successfully at: " << db_path << std::endl;

    // Seed default stock data if necessary
    StockDataManager::populate_database_if_empty(*db);

    int port = 8080;
    if (argc > 1) {
        try { port = std::stoi(argv[1]); } catch (...) {}
    }

    simple_http::Server server;
    server.set_static_dir(frontend_dir);
    std::cout << "[Static Server] Mounting frontend directory: " << frontend_dir << std::endl;

    // 1. GET /api/stocks
    server.Get("/api/stocks", [db](const simple_http::Request& req, simple_http::Response& res) {
        auto stocks = db->get_all_stocks();
        json j_arr = json::array();
        for (const auto& s : stocks) {
            j_arr.push_back(s.to_json());
        }
        json response = json::object();
        response["status"] = "success";
        response["count"] = static_cast<int>(stocks.size());
        response["data"] = j_arr;
        res.set_json(response);
    });

    // 2. GET /api/stocks/search
    server.Get("/api/stocks/search", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = req.get_param("symbol");
        std::string query = req.get_param("query");
        if (query.empty()) query = symbol;

        auto matches = db->search_stocks(query);
        json j_arr = json::array();
        for (const auto& s : matches) {
            j_arr.push_back(s.to_json());
        }
        json response = json::object();
        response["status"] = "success";
        response["query"] = query;
        response["count"] = static_cast<int>(matches.size());
        response["data"] = j_arr;
        res.set_json(response);
    });

    // 3. GET /api/stocks/history
    server.Get("/api/stocks/history", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = req.get_param("symbol", "TCS");
        std::string start_date = req.get_param("start");
        std::string end_date = req.get_param("end");

        auto prices = db->get_historical_prices(symbol, start_date, end_date);
        Stock stock_info;
        db->get_stock(symbol, stock_info);

        json j_arr = json::array();
        for (const auto& p : prices) {
            j_arr.push_back(p.to_json());
        }

        json response = json::object();
        response["status"] = "success";
        response["symbol"] = symbol;
        response["stock"] = stock_info.to_json();
        response["count"] = static_cast<int>(prices.size());
        response["data"] = j_arr;
        response["data_source"] = "Simulated / Yahoo Finance Demo Data (C++ Engine Verified)";
        res.set_json(response);
    });

    // 4. POST /api/data/preprocess
    server.Post("/api/data/preprocess", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = "TCS";
        std::string start_date = "";
        std::string end_date = "";
        try {
            json body_json = req.json_body();
            if (body_json.contains("symbol")) {
                symbol = body_json["symbol"].get<std::string>();
            }
            if (body_json.contains("start_date")) {
                start_date = body_json["start_date"].get<std::string>();
            }
            if (body_json.contains("end_date")) {
                end_date = body_json["end_date"].get<std::string>();
            }
        } catch (...) {}

        auto history = db->get_historical_prices(symbol, start_date, end_date);
        if (history.empty()) {
            history = db->get_historical_prices(symbol);
        }
        if (history.empty()) {
            res.status = 404;
            json err = json::object();
            err["status"] = "error";
            err["message"] = "No historical data found for symbol: " + symbol;
            res.set_json(err);
            return;
        }

        auto summary = PreprocessingEngine::clean_and_normalize(history, symbol);
        // Persist cleaned and deduplicated dataset
        db->batch_insert_prices(history);

        json response = json::object();
        response["status"] = "success";
        response["message"] = "Data cleaned, interpolated, and normalized successfully in C++ STL.";
        response["summary"] = summary.to_json();
        res.set_json(response);
    });

    // 5. POST /api/predict
    server.Post("/api/predict", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = "TCS";
        std::string algorithm = "linear_regression";
        int days = 7;

        try {
            json body = req.json_body();
            if (body.contains("symbol")) symbol = body["symbol"].get<std::string>();
            if (body.contains("algorithm")) algorithm = body["algorithm"].get<std::string>();
            if (body.contains("days")) days = body["days"].get<int>();
        } catch (...) {}

        if (days != 7 && days != 15 && days != 30) {
            days = 7;
        }

        auto history = db->get_historical_prices(symbol);
        if (history.empty()) {
            res.status = 404;
            json err = json::object();
            err["status"] = "error";
            err["message"] = "Historical records unavailable for " + symbol;
            res.set_json(err);
            return;
        }

        // Execute C++ predictive modeling algorithm
        auto forecast = PredictionEngine::generate_forecast(history, symbol, algorithm, days);

        // Store forecast into SQLite predictions table
        for (const auto& pt : forecast.forecast_points) {
            db->save_prediction(symbol, algorithm, pt.predicted_price, pt.date);
        }

        json response = json::object();
        response["status"] = "success";
        response["prediction"] = forecast.to_json();
        res.set_json(response);
    });

    // 6. GET /api/predictions
    server.Get("/api/predictions", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = req.get_param("symbol");
        int limit = 100;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param("limit")); } catch (...) {}
        }

        auto records = db->get_predictions(symbol, limit);
        json j_arr = json::array();
        for (const auto& r : records) {
            j_arr.push_back(r.to_json());
        }

        json response = json::object();
        response["status"] = "success";
        response["symbol"] = symbol.empty() ? "ALL" : symbol;
        response["count"] = static_cast<int>(records.size());
        response["data"] = j_arr;
        res.set_json(response);
    });

    // 7. GET /api/analytics
    server.Get("/api/analytics", [db](const simple_http::Request& req, simple_http::Response& res) {
        std::string symbol = req.get_param("symbol", "TCS");
        std::string start_date = req.get_param("start");
        std::string end_date = req.get_param("end");

        auto history = db->get_historical_prices(symbol, start_date, end_date);
        if (history.empty()) {
            // Fallback to all history if filtered slice is empty
            history = db->get_historical_prices(symbol);
        }
        if (history.empty()) {
            res.status = 404;
            json err = json::object();
            err["status"] = "error";
            err["message"] = "No historical data found for symbol: " + symbol;
            res.set_json(err);
            return;
        }

        auto indicators = AnalyticsEngine::compute_indicators(history, symbol);
        Stock stock_info;
        db->get_stock(symbol, stock_info);

        json response = json::object();
        response["status"] = "success";
        response["symbol"] = symbol;
        response["stock"] = stock_info.to_json();
        response["analytics"] = indicators.to_json();
        res.set_json(response);
    });

    // 8. GET /api/health
    server.Get("/api/health", [db, start_time](const simple_http::Request& req, simple_http::Response& res) {
        auto now = std::chrono::steady_clock::now();
        auto uptime_sec = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        json j = json::object();
        j["status"] = "healthy";
        j["uptime_seconds"] = static_cast<long long>(uptime_sec);
        j["backend_language"] = "C++17 (UCRT64 GCC)";
        j["database"] = "SQLite3 (Embedded)";
        j["database_connected"] = db->is_open();
        j["total_stocks"] = db->count_total_stocks();
        j["total_historical_prices"] = db->count_total_prices();
        j["total_predictions"] = db->count_total_predictions();
        j["features"] = json::array();
        j["features"].push_back("Data Ingestion & Cleaning (C++)");
        j["features"].push_back("Moving Average Forecasting (C++)");
        j["features"].push_back("Linear Regression Forecasting (C++)");
        j["features"].push_back("Exponential Moving Average Forecasting (C++)");
        j["features"].push_back("MAE, MSE, RMSE Validation Engine (C++)");
        j["features"].push_back("RSI, MACD, Bollinger Bands Technical Engine (C++)");
        j["features"].push_back("Transparent Buy/Sell/Hold Decision Engine (C++)");
        res.set_json(j);
    });

    // Start server with port fallback
    if (!server.listen_auto("0.0.0.0", port)) {
        std::cerr << "Failed to start HTTP server on candidate ports." << std::endl;
        return 1;
    }

    return 0;
}
