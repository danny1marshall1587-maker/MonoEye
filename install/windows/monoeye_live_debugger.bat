@echo off
setlocal EnableDelayedExpansion

:: MonoEye Live Debugger & Diagnostic Tool v1.2
:: This tool helps identify why MonoEye might fail to load in specific games.

title MonoEye Live Debugger

:: --- Configuration ---
:: We assume this script is in 'install/windows/' or root.
:: If in 'install/windows/', manifest is at '../../manifest/'
:: If in root, manifest is at 'manifest/'
set "SCRIPT_DIR=%~dp0"
if exist "%SCRIPT_DIR%manifest\XR_APILAYER_NOVENDOR_monoeye.json" (
    set "MANIFEST_PATH=%SCRIPT_DIR%manifest\XR_APILAYER_NOVENDOR_monoeye.json"
    set "LAYER_DIR=%SCRIPT_DIR%"
) else if exist "%SCRIPT_DIR%..\..\manifest\XR_APILAYER_NOVENDOR_monoeye.json" (
    set "MANIFEST_PATH=%SCRIPT_DIR%..\..\manifest\XR_APILAYER_NOVENDOR_monoeye.json"
    set "LAYER_DIR=%SCRIPT_DIR%..\..\"
) else (
    set "MANIFEST_PATH=NOT_FOUND"
)

set "LOG_FILE=%USERPROFILE%\Documents\MonoEye\monoeye.log"

:main_menu
cls
echo ======================================================================
echo   MONOEYE LIVE DEBUGGER ^& DIAGNOSTIC TOOL v1.2
echo ======================================================================
echo.
echo  [1] System Check (Verify Registry, Manifest, and DLL)
echo  [2] Start Live Logger (Tail monoeye.log)
echo  [3] FORCE LAUNCH Game (Auto-VR, Path Injection, Debug)
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
if exist "%LAYER_DIR%XR_APILAYER_NOVENDOR_monoeye.dll" (
    echo [OK] Layer DLL found: XR_APILAYER_NOVENDOR_monoeye.dll
) else (
    echo [!] ERROR: Layer DLL not found in expected directory: "%LAYER_DIR%"
)

:: Check Manifest
if exist "%MANIFEST_PATH%" (
    echo [OK] Manifest found: %MANIFEST_PATH%
) else (
    echo [!] ERROR: Manifest not found! Checked: "%MANIFEST_PATH%"
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
    ) else (
        echo [!] WARNING: MonoEye is NOT registered as an implicit layer.
        echo      This is fine if using the "FORCE LAUNCH" option.
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
echo [FORCE LAUNCHER] Preparing Session...
echo.

:: 1. Setup Layer Path Injection (Forces loader to see us)
if exist "%MANIFEST_PATH%" (
    set "XR_API_LAYER_PATH=%MANIFEST_PATH%"
    echo [+] FORCING Layer Path: !XR_API_LAYER_PATH!
) else (
    echo [!] ERROR: Cannot force launch - Manifest not found!
    pause
    goto main_menu
)

:: 2. Setup Verbose Debugging
set "MONOEYE_LOG_ENABLED=1"
set "MONOEYE_LOG_LEVEL=debug"
set "MONOEYE_ENABLE=1"
set "XR_LOADER_DEBUG=all"

echo [+] FORCING MONOEYE_LOG_LEVEL=debug
echo [+] FORCING XR_LOADER_DEBUG=all
echo.

:: 3. Application Path
echo Please DRAG AND DROP your Game EXE (e.g. acevo.exe) into this window:
set /p GAME_PATH="> "
set "GAME_PATH=%GAME_PATH:"=%"

if not exist "%GAME_PATH%" (
    echo [!] ERROR: Could not find "%GAME_PATH%"
    pause
    goto main_menu
)

:: 4. Force VR Arguments
echo.
echo Choose Launch Mode:
echo  [1] Standard VR (adds -vr)
echo  [2] OpenXR Mode (adds -openxr)
echo  [3] Custom / No extra args
echo.
set /p mode="Mode: "

set "ARGS="
if "%mode%"=="1" set "ARGS=-vr"
if "%mode%"=="2" set "ARGS=-openxr"

echo.
echo [+] Launching: "%GAME_PATH%" %ARGS%
echo.
echo ----------------------------------------------------------------------
echo   NOTE: Look at THIS WINDOW for OpenXR Loader logs.
echo   Internal MonoEye logs will go to Documents\MonoEye\monoeye.log
echo ----------------------------------------------------------------------
echo.

:: Launch the game. We use /B to keep the terminal attached to stderr/stdout
start /B "" "%GAME_PATH%" %ARGS%

echo.
echo [+] Game process started. 
echo [+] Starting log monitor in 5 seconds...
timeout /t 5 > nul

:: Switch to live logger but in the same window (don't cls)
echo ----------------------------------------------------------------------
echo   LIVE MONOEYE LOG START
echo ----------------------------------------------------------------------
powershell.exe -ExecutionPolicy Bypass -Command "if (Test-Path '$env:LOG_FILE') { Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 } else { Write-Host 'Waiting for log file...'; while (!(Test-Path '$env:LOG_FILE')) { Start-Sleep -Seconds 1 }; Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 }"

pause
goto main_menu

:clean_registry
cls
echo [CLEAN] Removing potential stale MonoEye entries...
reg delete "HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "XR_APILAYER_NOVENDOR_monoeye" /f >nul 2>&1
echo [OK] Cleaned.
pause
goto main_menu
