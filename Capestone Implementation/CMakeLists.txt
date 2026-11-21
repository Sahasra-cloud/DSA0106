cmake_minimum_required(VERSION 3.14)
project(StockPriceForecastingFramework VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Backend source files
set(BACKEND_SOURCES
    backend/main.cpp
    backend/stock_data.cpp
    backend/preprocessing.cpp
    backend/prediction.cpp
    backend/analytics.cpp
    backend/database.cpp
    backend/sqlite3.c
)

# Header include paths
set(BACKEND_INCLUDES
    backend/include
    backend
)

# Server executable target
add_executable(stock_server ${BACKEND_SOURCES})

target_include_directories(stock_server PRIVATE ${BACKEND_INCLUDES})

# Platform-specific socket linking
if(WIN32)
    target_link_libraries(stock_server PRIVATE ws2_32 wsock32)
else()
    target_link_libraries(stock_server PRIVATE pthread dl)
endif()

message(STATUS "Configured StockPriceForecastingFramework with C++17 and embedded SQLite3.")
