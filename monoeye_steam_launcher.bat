@echo off
setlocal EnableDelayedExpansion

:: MonoEye Steam/SteamVR Debug Launcher v1.0
:: This script launches the Steam ecosystem with forced MonoEye debug variables.

title MonoEye Steam/SteamVR Debug Launcher

echo ======================================================================
echo   MONOEYE STEAM/STEAMVR ECOSYSTEM LAUNCHER
echo ======================================================================
echo.

:: 1. Force Debug Variables for the entire process tree
echo [1/4] Preparing Environment...
set MONOEYE_LOG_ENABLED=1
set MONOEYE_LOG_LEVEL=debug
set MONOEYE_ENABLE=1
echo      [+] MONOEYE_LOG_ENABLED=1
echo      [+] MONOEYE_LOG_LEVEL=debug
echo      [+] MONOEYE_ENABLE=1
echo.

:: 2. Find Steam via Registry
echo [2/4] Detecting Steam...
for /f "tokens=2*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamExe 2^>nul') do set "STEAM_EXE=%%b"

if "%STEAM_EXE%"=="" (
    echo [!] Warning: Could not find Steam path in Registry.
    echo Please enter the path to steam.exe manually (or drag and drop it):
    set /p STEAM_EXE="> "
    set "STEAM_EXE=!STEAM_EXE:"=!"
) else (
    echo      [+] Found Steam: "%STEAM_EXE%"
)
echo.

:: 3. Verification & Launch
echo [3/4] Ready to Launch
echo ----------------------------------------------------------------------
echo IMPORTANT: You MUST close Steam and SteamVR completely before proceeding.
echo If they are already running, they will NOT see the debug settings.
echo ----------------------------------------------------------------------
echo.
echo Press any key to launch Steam and start monitoring logs...
pause > nul

echo.
echo [+] Launching Steam...
:: Starting Steam will ensure that SteamVR and any games launched from it 
:: inherit the MonoEye environment variables set above.
start "" "%STEAM_EXE%"

:: 4. Monitor Log
set "LOG_FILE=%USERPROFILE%\Documents\MonoEye\monoeye.log"
echo [4/4] Monitoring Log: "%LOG_FILE%"
echo.
echo ----------------------------------------------------------------------
echo   LIVE LOG MONITOR (Press Ctrl+C to stop)
echo ----------------------------------------------------------------------
echo Waiting for SteamVR or a Game to initialize MonoEye...
echo.

:: Wait for log file
:wait_loop
if not exist "%LOG_FILE%" (
    timeout /t 2 > nul
    goto wait_loop
)

:: Live tail using PowerShell
powershell.exe -ExecutionPolicy Bypass -Command "Get-Content -Path \"$env:LOG_FILE\" -Wait -Tail 15"

echo.
echo Log monitoring stopped.
pause
