#include "database.hpp"
#include <iostream>
#include <sstream>

StockDatabase::StockDatabase() : db(nullptr) {}

StockDatabase::~StockDatabase() {
    close();
}

bool StockDatabase::init(const std::string& path) {
    std::lock_guard<std::mutex> lock(db_mutex);
    db_path = path;
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    
    // Enable WAL mode for high concurrency
    char* err_msg = nullptr;
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg);
    if (err_msg) sqlite3_free(err_msg);

    return create_tables();
}

void StockDatabase::close() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool StockDatabase::create_tables() {
    if (!db) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS stocks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT UNIQUE NOT NULL,
            company_name TEXT NOT NULL,
            market TEXT NOT NULL,
            currency TEXT NOT NULL DEFAULT 'USD'
        );

        CREATE TABLE IF NOT EXISTS historical_prices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            date TEXT NOT NULL,
            open REAL NOT NULL,
            high REAL NOT NULL,
            low REAL NOT NULL,
            close REAL NOT NULL,
            volume INTEGER NOT NULL,
            change_percent REAL NOT NULL DEFAULT 0.0,
            UNIQUE(symbol, date)
        );

        CREATE TABLE IF NOT EXISTS predictions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            algorithm TEXT NOT NULL,
            predicted_price REAL NOT NULL,
            prediction_date TEXT NOT NULL,
            created_date TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_hist_symbol_date ON historical_prices(symbol, date);
        CREATE INDEX IF NOT EXISTS idx_pred_symbol ON predictions(symbol);
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error creating tables: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool StockDatabase::insert_stock(const Stock& stock) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    const char* sql = "INSERT OR REPLACE INTO stocks (symbol, company_name, market, currency) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, stock.symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, stock.company_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, stock.market.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, stock.currency.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Stock> StockDatabase::get_all_stocks() {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<Stock> list;
    if (!db) return list;

    const char* sql = "SELECT id, symbol, company_name, market, currency FROM stocks ORDER BY symbol ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Stock s;
        s.id = sqlite3_column_int(stmt, 0);
        s.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        s.company_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s.market = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        s.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        // Get latest price & change for this stock
        const char* price_sql = "SELECT close, change_percent FROM historical_prices WHERE symbol = ? ORDER BY date DESC LIMIT 1;";
        sqlite3_stmt* p_stmt = nullptr;
        if (sqlite3_prepare_v2(db, price_sql, -1, &p_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(p_stmt, 1, s.symbol.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(p_stmt) == SQLITE_ROW) {
                s.current_price = sqlite3_column_double(p_stmt, 0);
                s.daily_change_pct = sqlite3_column_double(p_stmt, 1);
                s.daily_change = s.current_price * (s.daily_change_pct / 100.0);
            }
            sqlite3_finalize(p_stmt);
        }

        list.push_back(s);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<Stock> StockDatabase::search_stocks(const std::string& query) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<Stock> list;
    if (!db) return list;

    std::string pattern = "%" + query + "%";
    const char* sql = "SELECT id, symbol, company_name, market, currency FROM stocks WHERE symbol LIKE ? OR company_name LIKE ? ORDER BY symbol ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Stock s;
        s.id = sqlite3_column_int(stmt, 0);
        s.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        s.company_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s.market = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        s.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        const char* price_sql = "SELECT close, change_percent FROM historical_prices WHERE symbol = ? ORDER BY date DESC LIMIT 1;";
        sqlite3_stmt* p_stmt = nullptr;
        if (sqlite3_prepare_v2(db, price_sql, -1, &p_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(p_stmt, 1, s.symbol.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(p_stmt) == SQLITE_ROW) {
                s.current_price = sqlite3_column_double(p_stmt, 0);
                s.daily_change_pct = sqlite3_column_double(p_stmt, 1);
                s.daily_change = s.current_price * (s.daily_change_pct / 100.0);
            }
            sqlite3_finalize(p_stmt);
        }

        list.push_back(s);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool StockDatabase::get_stock(const std::string& symbol, Stock& out_stock) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    const char* sql = "SELECT id, symbol, company_name, market, currency FROM stocks WHERE symbol = ? COLLATE NOCASE LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_stock.id = sqlite3_column_int(stmt, 0);
        out_stock.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out_stock.company_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out_stock.market = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out_stock.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        found = true;
    }
    sqlite3_finalize(stmt);

    if (found) {
        const char* price_sql = "SELECT close, change_percent FROM historical_prices WHERE symbol = ? ORDER BY date DESC LIMIT 1;";
        sqlite3_stmt* p_stmt = nullptr;
        if (sqlite3_prepare_v2(db, price_sql, -1, &p_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(p_stmt, 1, out_stock.symbol.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(p_stmt) == SQLITE_ROW) {
                out_stock.current_price = sqlite3_column_double(p_stmt, 0);
                out_stock.daily_change_pct = sqlite3_column_double(p_stmt, 1);
                out_stock.daily_change = out_stock.current_price * (out_stock.daily_change_pct / 100.0);
            }
            sqlite3_finalize(p_stmt);
        }
    }
    return found;
}

bool StockDatabase::insert_historical_price(const StockPrice& price) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    const char* sql = "INSERT OR REPLACE INTO historical_prices (symbol, date, open, high, low, close, volume, change_percent) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, price.symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, price.date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, price.open);
    sqlite3_bind_double(stmt, 4, price.high);
    sqlite3_bind_double(stmt, 5, price.low);
    sqlite3_bind_double(stmt, 6, price.close);
    sqlite3_bind_int64(stmt, 7, price.volume);
    sqlite3_bind_double(stmt, 8, price.change_percent);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool StockDatabase::batch_insert_prices(const std::vector<StockPrice>& prices) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    const char* sql = "INSERT OR REPLACE INTO historical_prices (symbol, date, open, high, low, close, volume, change_percent) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& price : prices) {
        sqlite3_bind_text(stmt, 1, price.symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, price.date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, price.open);
        sqlite3_bind_double(stmt, 4, price.high);
        sqlite3_bind_double(stmt, 5, price.low);
        sqlite3_bind_double(stmt, 6, price.close);
        sqlite3_bind_int64(stmt, 7, price.volume);
        sqlite3_bind_double(stmt, 8, price.change_percent);

        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

std::vector<StockPrice> StockDatabase::get_historical_prices(const std::string& symbol, const std::string& start_date, const std::string& end_date) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<StockPrice> list;
    if (!db) return list;

    std::ostringstream sql;
    sql << "SELECT id, symbol, date, open, high, low, close, volume, change_percent FROM historical_prices WHERE symbol = ? COLLATE NOCASE ";
    if (!start_date.empty()) sql << "AND date >= ? ";
    if (!end_date.empty()) sql << "AND date <= ? ";
    sql << "ORDER BY date ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    int param_idx = 1;
    sqlite3_bind_text(stmt, param_idx++, symbol.c_str(), -1, SQLITE_TRANSIENT);
    if (!start_date.empty()) sqlite3_bind_text(stmt, param_idx++, start_date.c_str(), -1, SQLITE_TRANSIENT);
    if (!end_date.empty()) sqlite3_bind_text(stmt, param_idx++, end_date.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StockPrice p;
        p.id = sqlite3_column_int(stmt, 0);
        p.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.open = sqlite3_column_double(stmt, 3);
        p.high = sqlite3_column_double(stmt, 4);
        p.low = sqlite3_column_double(stmt, 5);
        p.close = sqlite3_column_double(stmt, 6);
        p.volume = sqlite3_column_int64(stmt, 7);
        p.change_percent = sqlite3_column_double(stmt, 8);
        list.push_back(p);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool StockDatabase::save_prediction(const std::string& symbol, const std::string& algorithm, double predicted_price, const std::string& prediction_date) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    // Get current datetime string
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    std::string created_date = buf;

    const char* sql = "INSERT INTO predictions (symbol, algorithm, predicted_price, prediction_date, created_date) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, algorithm.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, predicted_price);
    sqlite3_bind_text(stmt, 4, prediction_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, created_date.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<PredictionResultRecord> StockDatabase::get_predictions(const std::string& symbol, int limit) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<PredictionResultRecord> list;
    if (!db) return list;

    std::ostringstream sql;
    sql << "SELECT id, symbol, algorithm, predicted_price, prediction_date, created_date FROM predictions ";
    if (!symbol.empty()) sql << "WHERE symbol = ? COLLATE NOCASE ";
    sql << "ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    int idx = 1;
    if (!symbol.empty()) {
        sqlite3_bind_text(stmt, idx++, symbol.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, idx, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PredictionResultRecord rec;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.algorithm = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.predicted_price = sqlite3_column_double(stmt, 3);
        rec.prediction_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.created_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        list.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool StockDatabase::clear_predictions(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return false;

    std::string sql = symbol.empty() ? "DELETE FROM predictions;" : "DELETE FROM predictions WHERE symbol = ? COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    if (!symbol.empty()) {
        sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int StockDatabase::count_total_stocks() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return 0;
    const char* sql = "SELECT COUNT(*) FROM stocks;";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

int StockDatabase::count_total_prices() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return 0;
    const char* sql = "SELECT COUNT(*) FROM historical_prices;";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

int StockDatabase::count_total_predictions() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) return 0;
    const char* sql = "SELECT COUNT(*) FROM predictions;";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}
