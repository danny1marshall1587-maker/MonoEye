@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   MonoEye Uninstaller
echo ========================================
echo.

:: Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This uninstaller must be run as Administrator.
    echo Right-click this file and select "Run as administrator".
    echo.
    pause
    exit /b 1
)

set "DEFAULT_INSTALL_DIR=%ProgramFiles%\MonoEye"
set "INSTALL_DIR=%DEFAULT_INSTALL_DIR%"

echo Uninstall from: %INSTALL_DIR%
echo.
set /p "CHOICE=Press Enter to confirm, or type a different path: "
if not "%CHOICE%"=="" set "INSTALL_DIR=%CHOICE%"

if not exist "%INSTALL_DIR%" (
    echo ERROR: Installation directory not found.
    pause
    exit /b 1
)

echo Removing environment variables...
setx MONOEYE_ENABLE "" /M >nul 2>&1
setx MONOEYE_DISABLE "" /M >nul 2>&1
setx MONOEYE_QUALITY "" /M >nul 2>&1
setx MONOEYE_LEFT_EYE "" /M >nul 2>&1
setx MONOEYE_LOG_LEVEL "" /M >nul 2>&1
echo   - Environment variables cleared

echo.
echo Removing desktop shortcut...
del "%PUBLIC%\Desktop\MonoEye Switcher.lnk" 2>nul
del "%USERPROFILE%\Desktop\MonoEye Switcher.lnk" 2>nul
echo   - Shortcut removed

echo.
echo Removing installation directory...
rmdir /s /q "%INSTALL_DIR%"
if !errorLevel! equ 0 (
    echo   - Installation directory removed
) else (
    echo   WARNING: Failed to remove installation directory
    echo   You may need to manually delete: %INSTALL_DIR%
)

echo.
echo ========================================
echo   Uninstallation Complete
echo ========================================
echo.
echo MonoEye has been removed from your system.
echo VR games will now use normal dual-eye rendering.
echo.
pause
