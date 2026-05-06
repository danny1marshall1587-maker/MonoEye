@echo off
setlocal EnableDelayedExpansion

:: MonoEye Diagnostic & Logging Tool v1.2
:: Simple version to ensure compatibility.

title MonoEye Diagnostic Tool

echo ======================================================================
echo   MONOEYE DIAGNOSTIC TOOL
echo ======================================================================
echo Current Path: %~dp0
echo.

echo [1] Checking MonoEye Environment Variables...
echo ----------------------------------------------------------------------
set MONOEYE_ 2>nul
if %ERRORLEVEL% NEQ 0 echo No MONOEYE_* variables found in this session.
echo.

echo [2] Checking Registry (Implicit API Layers)...
echo ----------------------------------------------------------------------
set "LAYER_KEY=SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
echo HKLM:
reg query "HKLM\%LAYER_KEY%" /s 2>nul | findstr /i "monoeye"
echo HKCU:
reg query "HKCU\%LAYER_KEY%" /s 2>nul | findstr /i "monoeye"
echo.

echo [3] Checking Log File...
echo ----------------------------------------------------------------------
:: Check common paths
set "LOG_FILE=%USERPROFILE%\Documents\MonoEye\monoeye.log"
if exist "%LOG_FILE%" (
    echo [+] Found log at: "%LOG_FILE%"
    echo.
    echo --- LOG CONTENT ---
    type "%LOG_FILE%"
    echo --- END OF LOG ---
) else (
    echo [-] Log file NOT FOUND at: "%LOG_FILE%"
)
echo.

echo ======================================================================
echo DIAGNOSTIC COMPLETE. 
echo If the window closes immediately, please run this from a CMD prompt.
echo ======================================================================
pause
