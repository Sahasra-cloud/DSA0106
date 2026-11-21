@echo off
setlocal
echo ====================================================================
echo  Stock Forecasting Analytical Framework - Build ^& Launch Script
echo ====================================================================

echo [0/3] Stopping any previous servers on port 8080...
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :8080 ^| findstr LISTENING') do (
    taskkill /F /PID %%a >nul 2>&1
)

cd /d "%~dp0backend"

if not exist "sqlite3.o" (
    echo [1/3] Compiling embedded SQLite3 amalgamation...
    gcc -O2 -c sqlite3.c -o sqlite3.o
) else (
    echo [1/3] Using cached sqlite3.o object...
)

echo [2/3] Compiling C++ backend server...
g++ -O2 -std=c++17 -Iinclude main.cpp stock_data.cpp preprocessing.cpp prediction.cpp analytics.cpp database.cpp sqlite3.o -o stock_server.exe -lws2_32

if errorlevel 1 (
    echo.
    echo [ERROR] C++ compilation failed. Ensure g++ (MinGW/MSYS2) is installed on PATH.
    pause
    exit /b 1
)

echo.
echo [3/3] Starting C++ REST API Server on http://localhost:8080/ ...
echo Web Terminal: http://localhost:8080/
echo API Health:   http://localhost:8080/api/health
echo.
start http://localhost:8080/
stock_server.exe 8080

pause
