@echo off
setlocal

if /I "%~1"=="open-browser" goto open_browser

cd /d "%~dp0"
set "PORT=4175"
set "URL=http://127.0.0.1:%PORT%/"

if not exist "package.json" (
    echo package.json not found.
    pause
    exit /b 1
)

if not exist "node_modules" (
    echo node_modules not found. Run npm install first.
    pause
    exit /b 1
)

netstat -ano | findstr /R /C:"127.0.0.1:%PORT% .*LISTENING" >nul
if not errorlevel 1 (
    echo Tutorial site is already running.
    echo Opening: %URL%
    start "" "%URL%"
    exit /b 0
)

echo.
echo Starting WLC tutorial site...
echo URL: %URL%
echo Stop: press Ctrl+C in this window
echo.

start "" /min cmd /c ""%~f0" open-browser"
call npm.cmd run dev -- --host 127.0.0.1 --port %PORT% --strictPort
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo.
    echo Start failed. Exit code: %EXIT_CODE%
    pause
)

exit /b %EXIT_CODE%

:open_browser
cd /d "%~dp0"
set "PORT=4175"
set "URL=http://127.0.0.1:%PORT%/"
set /a WAIT_COUNT=0

:wait_for_server
timeout /t 1 /nobreak >nul
set /a WAIT_COUNT+=1
netstat -ano | findstr /R /C:"127.0.0.1:%PORT% .*LISTENING" >nul

if not errorlevel 1 (
    start "" "%URL%"
    exit /b 0
)

if %WAIT_COUNT% GEQ 15 (
    exit /b 1
)

goto wait_for_server
