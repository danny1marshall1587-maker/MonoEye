@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   MonoEye Absolute Cleanup Tool
echo ===================================================
echo This utility will PURGE all MonoEye registrations
echo and environment variables from your system.
echo.
echo WARNING: Run this ONLY if you are doing a clean install.
echo.

:: Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo ERROR: This script MUST be run as Administrator.
    echo Please right-click and "Run as Administrator".
    echo.
    pause
    exit /b 1
)

echo.
echo [1/3] Removing MonoEye Registry Entries...

:: Define all possible OpenXR layer registry paths
set "KEYS="
set "KEYS=!KEYS! HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"
set "KEYS=!KEYS! HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Explicit"

for %%K in (%KEYS%) do (
    echo   - Scrubbing %%K...
    for /f "tokens=*" %%V in ('reg query "%%K" 2^>nul ^| findstr /i "monoeye"') do (
        echo     Removing: %%V
        reg delete "%%K" /v "%%V" /f >nul 2>&1
    )
)

echo.
echo [2/3] Cleaning Environment Variables...
:: List of all known MonoEye environment variables
set "VARS=MONOEYE_ENABLE MONOEYE_QUALITY MONOEYE_LEFT_EYE MONOEYE_INDICATOR MONOEYE_TENSOR_STABILIZATION MONOEYE_SPECULAR_REJECTION MONOEYE_EDGE_SMOOTHING MONOEYE_LOG_ENABLED MONOEYE_LOG_LEVEL MONOEYE_AMS2_STABILIZE MONOEYE_DISABLE XR_API_LAYER_PATH"

for %%V in (%VARS%) do (
    echo   - Clearing %%V...
    reg delete "HKCU\Environment" /v %%V /f >nul 2>&1
    reg delete "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v %%V /f >nul 2>&1
    set %%V=
)

echo.
echo [3/3] Cleanup complete.
echo.
echo ===================================================
echo   IMPORTANT: MANUAL STEPS REQUIRED
echo ===================================================
echo This tool cleared the registry, but you must now:
echo.
echo 1. DELETE all your old "MonoEye" folders manually.
echo 2. Extract the NEW v0.5.17 zip to a fresh folder.
echo 3. Run the Switcher from the new folder.
echo.
pause
