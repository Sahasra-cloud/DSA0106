@echo off
echo Stopping any previous servers on port 8080...
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :8080 ^| findstr LISTENING') do (
    taskkill /F /PID %%a >nul 2>&1
)
cd /d "%~dp0backend"
if exist "stock_server.exe" (
    echo Starting Stock Forecasting C++ Server...
    stock_server.exe 8080
) else (
    echo Executable not found. Running build_and_run.bat...
    call "%~dp0build_and_run.bat"
)
