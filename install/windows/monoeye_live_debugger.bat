@echo off
setlocal EnableDelayedExpansion

:: MonoEye Automated Debugger & VR Launcher v1.3
:: Automatically triggers when a game EXE is dropped onto this script.

title MonoEye Automated Debugger

:: --- Configuration ---
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

:: --- Auto-Launch Mode (if argument provided) ---
if not "%~1"=="" (
    set "GAME_PATH=%~1"
    goto auto_launch
)

:main_menu
cls
echo ======================================================================
echo   MONOEYE LIVE DEBUGGER ^& DIAGNOSTIC TOOL v1.3
echo ======================================================================
echo.
echo  TIP: You can DRAG AND DROP a game EXE directly onto this .bat file
echo       to launch it automatically with full VR debugging enabled.
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
if "%opt%"=="3" goto manual_launch
if "%opt%"=="4" goto clean_registry
if "%opt%"=="5" exit /b
goto main_menu

:auto_launch
cls
echo [AUTO-LAUNCH] Initializing MonoEye Force-VR Debug...
echo.
echo Target: "%GAME_PATH%"
echo.

:: 1. Path Injection
if exist "%MANIFEST_PATH%" (
    set "XR_API_LAYER_PATH=%MANIFEST_PATH%"
    echo [+] Path Injection: ACTIVE
) else (
    echo [!] WARNING: Manifest not found! Attempting launch anyway...
)

:: 2. Debugging
set "MONOEYE_LOG_ENABLED=1"
set "MONOEYE_LOG_LEVEL=debug"
set "MONOEYE_ENABLE=1"
set "XR_LOADER_DEBUG=all"

:: 3. VR Arguments (Force AC Evo and others into VR)
set "ARGS=-vr -openxr"

echo [+] Debugging: VERBOSE
echo [+] Mode: FORCE VR (added %ARGS%)
echo.
echo ----------------------------------------------------------------------
echo   LAUNCHING GAME...
echo   OpenXR Loader logs will appear BELOW.
echo   MonoEye logs go to Documents\MonoEye\monoeye.log
echo ----------------------------------------------------------------------
echo.

:: Use start /B to keep stdout/stderr attached to this window
start /B "" "%GAME_PATH%" %ARGS%

timeout /t 5 > nul
goto live_logger_stream

:manual_launch
cls
echo Please DRAG AND DROP your Game EXE into this window:
set /p GAME_PATH="> "
set "GAME_PATH=!GAME_PATH:"=!"
goto auto_launch

:system_check
:: ... (same as v1.2)
goto main_menu

:live_logger
cls
echo [LOGGER] Monitoring "%LOG_FILE%"...
goto live_logger_stream

:live_logger_stream
powershell.exe -ExecutionPolicy Bypass -Command "if (Test-Path '$env:LOG_FILE') { Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 } else { Write-Host 'Waiting for MonoEye log...'; while (!(Test-Path '$env:LOG_FILE')) { Start-Sleep -Seconds 1 }; Get-Content -Path '$env:LOG_FILE' -Wait -Tail 20 }"
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
