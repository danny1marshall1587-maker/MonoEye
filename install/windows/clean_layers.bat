@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   OpenXR API Layer Cleanup Utility
echo ===================================================
echo This utility will UNREGISTER all OpenXR API layers 
echo from your Windows Registry to resolve conflicts.
echo.
echo This affects:
echo   - Old MonoEye versions
echo   - Current MonoEye installation
echo   - SteamVR API layers (will re-register on next run)
echo   - Other 3rd party layers
echo.
echo WARNING: This will NOT delete any files, only unregister
echo the layers from the OpenXR loader.
echo.
pause

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
echo [1/3] Cleaning OpenXR Registry Keys...

:: Define all possible OpenXR layer registry paths
set "KEYS="
set "KEYS=!KEYS! HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"
set "KEYS=!KEYS! HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit"
set "KEYS=!KEYS! HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Explicit"

for %%K in (%KEYS%) do (
    echo   - Clearing %%K...
    :: Delete the key and all its values
    reg delete "%%K" /f >nul 2>&1
    :: Recreate the empty key so the loader doesn't complain about missing keys (though it should be fine)
    reg add "%%K" /f >nul 2>&1
)

echo.
echo [2/3] Cleaning MonoEye Environment Variables...
:: List of all known MonoEye environment variables
set "VARS=MONOEYE_ENABLE MONOEYE_QUALITY MONOEYE_LEFT_EYE MONOEYE_INDICATOR MONOEYE_TENSOR_STABILIZATION MONOEYE_SPECULAR_REJECTION MONOEYE_EDGE_SMOOTHING MONOEYE_LOG_ENABLED MONOEYE_LOG_LEVEL MONOEYE_AMS2_STABILIZE MONOEYE_DISABLE"

for %%V in (%VARS%) do (
    echo   - Removing %%V...
    :: Remove from System (requires admin, which we have)
    setx %%V "" /M >nul 2>&1
    :: Remove from User
    setx %%V "" >nul 2>&1
    :: Clear from current session
    set %%V=
)

echo.
echo [3/3] Checking for forced API Layer paths...
setx XR_API_LAYER_PATH "" /M >nul 2>&1
setx XR_API_LAYER_PATH "" >nul 2>&1
set XR_API_LAYER_PATH=

echo.
echo ===================================================
echo   CLEANUP COMPLETE
echo ===================================================
echo Your Windows environment has been cleaned of OpenXR 
echo API layer registrations.
echo.
echo NEXT STEPS:
echo 1. (Optional) Restart your computer.
echo 2. Launch SteamVR - it will automatically re-register
echo    its own required layers.
echo 3. Perform your clean install of MonoEye.
echo.
pause
