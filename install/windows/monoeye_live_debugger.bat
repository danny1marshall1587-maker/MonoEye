@echo off
setlocal EnableDelayedExpansion

:: MonoEye Live Debugger & Diagnostic Tool v1.1
:: This tool helps identify why MonoEye might fail to load in specific games.

title MonoEye Live Debugger

:: --- Configuration ---
set "MANIFEST_DIR=%~dp0manifest"
set "LAYER_DIR=%~dp0"
set "LOG_FILE=%USERPROFILE%\Documents\MonoEye\monoeye.log"

:main_menu
cls
echo ======================================================================
echo   MONOEYE LIVE DEBUGGER ^& DIAGNOSTIC TOOL
echo ======================================================================
echo.
echo  [1] System Check (Verify Registry, Manifest, and DLL)
echo  [2] Start Live Logger (Tail monoeye.log)
echo  [3] Launch Game with Full Debug (AC Evo / Steam / Generic)
echo  [4] Clean Registry (Remove broken OpenXR layers)
echo  [5] Exit
echo.
set /p opt="Select an option: "

if "%opt%"=="1" goto system_check
if "%opt%"=="2" goto live_logger
if "%opt%"=="3" goto launch_game
if "%opt%"=="4" goto clean_registry
if "%opt%"=="5" exit /b
goto main_menu

:system_check
cls
echo [DIAGNOSTIC] Checking System State...
echo.

:: Check for DLL
if exist "%~dp0XR_APILAYER_NOVENDOR_monoeye.dll" (
    echo [OK] Layer DLL found: XR_APILAYER_NOVENDOR_monoeye.dll
) else (
    echo [!] ERROR: Layer DLL not found in script directory!
)

:: Check Manifest
set "MANIFEST_PATH=%~dp0manifest\XR_APILAYER_NOVENDOR_monoeye.json"
if exist "%MANIFEST_PATH%" (
    echo [OK] Manifest found: %MANIFEST_PATH%
) else (
    echo [!] ERROR: Manifest not found!
)

:: Check Registry
echo [REG] Checking OpenXR API Layers in Registry...
reg query "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MonoEye found in HKLM Registry.
) else (
    reg query "HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] MonoEye found in HKCU Registry.
    else (
        echo [!] WARNING: MonoEye is NOT registered as an implicit layer.
        echo      Use the install.bat or Switcher to register it.
    )
)

echo.
echo [INFO] Current OpenXR Runtime:
reg query "HKLM\SOFTWARE\Khronos\OpenXR\1" /v "ActiveRuntime"
echo.
pause
goto main_menu

:live_logger
cls
echo [LOGGER] Monitoring "%LOG_FILE%"...
echo (Press Ctrl+C to stop)
echo.

if not exist "%LOG_FILE%" (
    echo [!] Log file does not exist yet. Waiting for game launch...
)

:: Use PowerShell to tail the log
powershell.exe -ExecutionPolicy Bypass -Command "if (Test-Path '$env:LOG_FILE') { Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 } else { Write-Host 'Waiting for log file...'; while (!(Test-Path '$env:LOG_FILE')) { Start-Sleep -Seconds 1 }; Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 }"
pause
goto main_menu

:launch_game
cls
echo [LAUNCHER] Forcing Debug Mode...
echo.
set MONOEYE_LOG_ENABLED=1
set MONOEYE_LOG_LEVEL=debug
set MONOEYE_ENABLE=1
set XR_LOADER_DEBUG=all

echo [DEBUG SET] MONOEYE_LOG_LEVEL=debug
echo [DEBUG SET] XR_LOADER_DEBUG=all (Verbose OpenXR Loader logs)
echo.
echo NOTE: Loader logs (XR_LOADER_DEBUG) will appear in the game's output.
echo MonoEye internal logs will go to %LOG_FILE%.
echo.
echo Please DRAG AND DROP your Game EXE into this window and press ENTER:
set /p GAME_PATH="> "
set "GAME_PATH=%GAME_PATH:"=%"

if not exist "%GAME_PATH%" (
    echo [!] ERROR: Could not find "%GAME_PATH%"
    pause
    goto main_menu
)

echo.
echo [+] Launching Game...
start "" "%GAME_PATH%"

echo.
echo [+] Game launched. Switching to live log monitor in 3 seconds...
timeout /t 3 > nul
goto live_logger

:clean_registry
cls
echo [CLEAN] Removing potential stale MonoEye entries...
reg delete "HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" /f >nul 2>&1
echo [OK] Cleaned. Please re-run install.bat or use Switcher to re-enable.
pause
goto main_menu
