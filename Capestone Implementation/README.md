# Design and Implementation of a Web-Based Analytical Framework for Daily Stock Price Forecasting

A production-grade, responsive financial analytics and stock price forecasting web application built with a **native C++17 backend**, embedded **SQLite3 database**, and a modern **HTML5, CSS3, JavaScript (Bootstrap 5, Chart.js)** financial terminal interface.

---

## 1. Project Overview & Architectural Highlights

This application bridges high-performance systems programming with intuitive financial data visualization:
- **Zero Python or Node.js Backend**: All data processing, missing-value interpolation, temporal deduplication, statistical normalization, technical indicators, and multi-day predictive modeling are implemented **strictly in native C++**.
- **Embedded SQLite3 Engine**: Direct low-latency persistence for company records, historical daily OHLCV prices, and audited prediction results.
- **RESTful JSON Communication**: Lightweight, multi-threaded C++ HTTP server providing standard REST API endpoints with cross-origin resource sharing (CORS) support.
- **Dark Financial Terminal Theme**: Responsive financial UI styled in dark navy blue (`#070d18`, `#0e172a`), neon blue/cyan highlights, and distinct bullish green / bearish red signal badges.

---

## 2. Technology Stack

### Frontend:
- **HTML5 & CSS3**: Custom modern financial theme with responsive CSS variables, glowing borders, and animations.
- **JavaScript (ES6+)**: Asynchronous REST API integration, dynamic DOM rendering, and CSV report export.
- **Bootstrap 5.3.2**: Responsive grid layout and UI containers.
- **Chart.js 4.4.1**: Interactive financial line charts, volume histograms, and 95% confidence band overlays.
- **Font Awesome 6.5.1**: Financial and metric iconography.

### Backend:
- **C++17 (UCRT64 MinGW GCC / MSVC)**: Core business logic, mathematical algorithms, and statistical modeling.
- **Native Multi-Threaded HTTP REST Server**: Custom WinSock2 (`ws2_32`) HTTP server handling concurrent requests and serving static frontend files.
- **C++ Standard Template Library (STL)**: Vector manipulation, map lookups, math algorithms, and chronological sorting.
- **Single-Header JSON Engine**: Fast, dependency-free C++ JSON parser and serializer.

### Database:
- **SQLite3 (Amalgamation embedded)**:
  - `stocks` table (Symbol, Company Name, Market, Currency)
  - `historical_prices` table (Date, Open, High, Low, Close, Volume, Daily Return %)
  - `predictions` table (Symbol, Algorithm, Predicted Price, Target Date, Created Timestamp)

---

## 3. System Modules

### Module 1: Data Ingestion and Preprocessing
- **Stock Coverage**: TCS, Infosys (INFY), Reliance Industries (RELIANCE), Apple (AAPL), Google (GOOGL), Microsoft (MSFT).
- **Data Cleaning**: Chronological date sorting, deduplication, and automated forward-fill/interpolation of missing or corrupted values.
- **Statistical Normalization**:
  - **Min-Max Normalization**:
    $$x' = \frac{x - x_{\min}}{x_{\max} - x_{\min}} \in [0, 1]$$
  - **Z-Score Standardization**:
    $$z = \frac{x - \mu}{\sigma}$$
- **Return Calculation**: Daily inter-day percentage returns:
  $$\Delta \% = \left(\frac{\text{Close}_t - \text{Close}_{t-1}}{\text{Close}_{t-1}}\right) \times 100$$

### Module 2: Predictive Modelling and Analytical Engine
- **Prediction Algorithms**:
  1. **Simple Moving Average (SMA)**: Rolling autoregressive window with momentum decay:
     $$\text{SMA}_t = \frac{1}{W}\sum_{i=1}^{W} P_{t-i}$$
  2. **Linear Regression (OLS)**: Ordinary Least Squares fitting time steps $x$ against close prices $y$:
     $$y = \alpha + \beta x, \quad \beta = \frac{\sum (x_i - \bar{x})(y_i - \bar{y})}{\sum (x_i - \bar{x})^2}, \quad \alpha = \bar{y} - \beta \bar{x}$$
  3. **Exponential Moving Average (EMA)**: Exponentially smoothed recency with momentum extrapolation:
     $$\text{EMA}_t = \alpha P_t + (1 - \alpha)\text{EMA}_{t-1}, \quad \alpha = \frac{2}{N + 1}$$
- **Forecast Horizons**: 7 Days, 15 Days, and 30 Days.
- **Holdout Validation Error Metrics**:
  - **Mean Absolute Error (MAE)**:
    $$\text{MAE} = \frac{1}{M}\sum_{i=1}^M |y_i - \hat{y}_i|$$
  - **Mean Squared Error (MSE)**:
    $$\text{MSE} = \frac{1}{M}\sum_{i=1}^M (y_i - \hat{y}_i)^2$$
  - **Root Mean Squared Error (RMSE)**:
    $$\text{RMSE} = \sqrt{\text{MSE}}$$
  - **Coefficient of Determination ($R^2$)**:
    $$R^2 = 1 - \frac{\sum (y_i - \hat{y}_i)^2}{\sum (y_i - \bar{y})^2}$$
- **Technical Indicators**:
  - 20-Day & 50-Day Simple Moving Averages
  - 12-Day & 26-Day Exponential Moving Averages
  - **RSI (14-Day)**: Wilder's Smoothed Relative Strength Index with 30 (Oversold) and 70 (Overbought) boundaries.
  - **MACD**: MACD Line, 9-Day Signal Line, and MACD Histogram.
  - **Bollinger Bands**: 20-Day SMA $\pm 2\sigma$ envelope.
  - **Annualized Historical Volatility**: $\sigma_{\text{daily}} \times \sqrt{252} \times 100\%$.
- **Transparent Decision Engine**: Rule-based synthesis yielding **BUY**, **SELL**, or **HOLD** ratings with human-readable rationale.

### Module 3: Web-Based Visualisation & Reporting
- 6 interactive web pages with real-time API binding.
- Chart.js interactive graphs with 95% confidence intervals.
- Live ticker marquee across global markets.
- One-click CSV export for historical datasets and multi-day prediction reports.

---

## 4. Website Pages

| Page | File | Description |
|---|---|---|
| **Home Page** | `frontend/index.html` | Hero introduction, quick stock search bar, featured stock cards, and 3-module architecture highlights. |
| **Dashboard** | `frontend/dashboard.html` | High-level market overview, active stock stats, 52-week extremes, historical price chart, 7-day forecast preview, and recent prices table. |
| **Stock Analysis** | `frontend/analysis.html` | In-depth technical analysis, date range filters, OHLCV dataset viewer, RSI/MACD/Bollinger Bands metrics, and C++ Preprocessing Lab (Min-Max & Z-Score). |
| **Prediction Lab** | `frontend/prediction.html` | Forecast workbench: select stock, algorithm (SMA / OLS / EMA), duration (7, 15, 30 days), interactive forecast chart with 95% confidence band, MAE/MSE/RMSE/R² metrics, and CSV export. |
| **Reports Center** | `frontend/reports.html` | Audit log of predictions saved in SQLite, algorithm accuracy benchmark table, and CSV downloads. |
| **About Project** | `frontend/about.html` | Academic objectives, complete module specifications, mathematical formulas, and educational disclaimer. |

---

## 5. Backend REST API Reference

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/health` | Backend status, uptime, SQLite connectivity, and feature audit. |
| `GET` | `/api/stocks` | List all available stocks with prices and 24h percentage changes. |
| `GET` | `/api/stocks/search?query=TCS` | Search stocks by symbol or company name. |
| `GET` | `/api/stocks/history?symbol=TCS` | Retrieve historical OHLCV records (supports `start` and `end` date parameters). |
| `POST` | `/api/data/preprocess` | Clean, deduplicate, interpolate, and normalize datasets in C++ (`{"symbol":"TCS"}`). |
| `POST` | `/api/predict` | Execute C++ forecasting algorithm (`{"symbol":"TCS","algorithm":"linear_regression","days":7}`). Saves predictions to SQLite. |
| `GET` | `/api/predictions?symbol=TCS` | Retrieve past prediction records from SQLite. |
| `GET` | `/api/analytics?symbol=TCS` | Calculate RSI, MACD, Bollinger Bands, Moving Averages, and Buy/Sell/Hold rating. |

---

## 6. Project Directory Structure

```
stock-forecasting/
│
├── frontend/
│   ├── index.html               # Home page
│   ├── dashboard.html           # Main analytics dashboard
│   ├── analysis.html            # In-depth analysis & preprocessing
│   ├── prediction.html          # Forecasting workbench
│   ├── reports.html             # Reports & SQLite prediction history
│   ├── about.html               # Architecture & project documentation
│   ├── css/
│   │   └── style.css            # Dark financial theme styles
│   └── js/
│       ├── app.js               # API client, ticker marquee, global helpers
│       ├── dashboard.js         # Dashboard view controller
│       ├── charts.js            # Chart.js visualization wrappers
│       └── prediction.js        # Forecasting controller & CSV export
│
├── backend/
│   ├── main.cpp                 # Server entry point & REST endpoints
│   ├── stock_data.cpp           # Stock dataset generator & seeder
│   ├── preprocessing.cpp        # Data cleaning, deduplication, normalization
│   ├── prediction.cpp           # SMA, OLS Linear Regression, EMA algorithms
│   ├── analytics.cpp            # RSI, MACD, Bollinger Bands, Rule Engine
│   ├── database.cpp             # SQLite3 database operations
│   ├── sqlite3.c                # SQLite3 amalgamation source
│   ├── sqlite3.o                # Pre-compiled SQLite object for fast builds
│   └── include/
│       ├── json.hpp             # Modern C++ JSON parser & serializer
│       ├── http_server.hpp      # Multi-threaded WinSock2 REST server
│       ├── models.hpp           # Data structures and domain models
│       ├── database.hpp         # Database class declaration
│       ├── stock_data.hpp       # Stock manager declaration
│       ├── preprocessing.hpp    # Preprocessing engine declaration
│       ├── prediction.hpp       # Prediction engine declaration
│       ├── analytics.hpp        # Analytics engine declaration
│       └── sqlite3.h            # SQLite3 header
│
├── database/
│   └── stocks.db                # SQLite3 persistent database file
│
├── CMakeLists.txt               # CMake build configuration
├── build_and_run.bat            # One-click Windows build and launch script
├── run_backend.bat              # Quick server launcher
└── README.md                    # Project documentation
```

---

## 7. How to Build and Run Locally on Windows (VS Code)

### Prerequisites:
- Windows 10 or 11
- MinGW-w64 GCC (`g++`) installed and available on your `PATH` (such as MSYS2 UCRT64 or WinLibs). Verify with:
  ```powershell
  g++ --version
  ```

### Method 1: One-Click Batch Script (Recommended)
Double-click `build_and_run.bat` (or execute in terminal):
```cmd
.\build_and_run.bat
```
This script will:
1. Compile `sqlite3.o` if not already built.
2. Compile the C++ backend into `stock_server.exe` with `-O2 -std=c++17 -lws2_32`.
3. Start the server on port `8080`.
4. Automatically open your browser to `http://localhost:8080/`.

---

### Method 2: Manual Terminal Compilation with g++
Navigate to the `backend` folder and compile:
```powershell
cd backend
g++ -O2 -std=c++17 -Iinclude main.cpp stock_data.cpp preprocessing.cpp prediction.cpp analytics.cpp database.cpp sqlite3.o -o stock_server.exe -lws2_32
.\stock_server.exe 8080
```
Then open your browser and navigate to:
```
http://localhost:8080/
```

---

### Method 3: CMake Build (Optional)
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
.\stock_server.exe 8080
```

---

## 8. Educational Disclaimer

> **DISCLAIMER**: This application is strictly an educational and analytical research demonstration. Stock market prices are inherently volatile and driven by macroeconomic events. All forecasts and Buy/Sell/Hold analytical signals are mathematical approximations and do not constitute financial, investment, or trading advice.
