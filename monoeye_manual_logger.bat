@echo off
setlocal EnableDelayedExpansion

:: MonoEye Manual Logging Launcher v1.0
:: This script bypasses the Switcher and forces debug logging for a game session.

title MonoEye Manual Logging Session

echo ======================================================================
echo   MONOEYE MANUAL LOGGING LAUNCHER
echo ======================================================================
echo.

:: 1. Force Debug Variables for this session only
echo [1/3] Forcing Debug Environment Variables...
set MONOEYE_LOG_ENABLED=1
set MONOEYE_LOG_LEVEL=debug
set MONOEYE_ENABLE=1
echo      [+] MONOEYE_LOG_ENABLED=1
echo      [+] MONOEYE_LOG_LEVEL=debug
echo      [+] MONOEYE_ENABLE=1
echo.

:: 2. Identify Log File
set "LOG_FILE=%USERPROFILE%\Documents\MonoEye\monoeye.log"
echo [2/3] Monitoring Log File:
echo      "%LOG_FILE%"
if not exist "%USERPROFILE%\Documents\MonoEye" mkdir "%USERPROFILE%\Documents\MonoEye"
echo.

:: 3. Application Launch
echo [3/3] Application Launch
echo ----------------------------------------------------------------------
echo Please DRAG AND DROP your Game EXE into this window and press ENTER:
echo (e.g. SteamLibrary\steamapps\common\MyGame\game.exe)
echo ----------------------------------------------------------------------
set /p GAME_PATH="> "

:: Remove quotes from drag-and-drop path
set "GAME_PATH=%GAME_PATH:"=%"

if not exist "%GAME_PATH%" (
    echo.
    echo [!] ERROR: Could not find the file: "%GAME_PATH%"
    echo Please make sure the path is correct and try again.
    pause
    exit /b
)

echo.
echo [+] Launching Game...
:: Using 'start' ensures the game inherits these specific environment variables
start "" "%GAME_PATH%"

echo.
echo ----------------------------------------------------------------------
echo   LIVE LOG MONITORING (Press Ctrl+C to exit monitor)
echo ----------------------------------------------------------------------
echo Waiting for game to start and MonoEye to initialize...
echo.

:: Wait loop to ensure the log file exists before trying to tail it
:wait_loop
if not exist "%LOG_FILE%" (
    timeout /t 1 > nul
    goto wait_loop
)

:: Live tail using PowerShell. Using $env:LOG_FILE to ensure path is passed correctly.
powershell.exe -ExecutionPolicy Bypass -Command "Get-Content -Path \"$env:LOG_FILE\" -Wait -Tail 15"

echo.
echo Log monitoring stopped.
pause
