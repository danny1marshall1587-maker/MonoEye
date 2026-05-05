@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   MonoEye Installer v0.5.8
echo   Single-eye VR rendering
echo ========================================
echo.

:: Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This installer must be run as Administrator.
    echo Right-click this file and select "Run as administrator".
    echo.
    pause
    exit /b 1
)

:: Determine install directory
set "DEFAULT_INSTALL_DIR=%ProgramFiles%\MonoEye"
set "INSTALL_DIR=%DEFAULT_INSTALL_DIR%"

echo Install to: %INSTALL_DIR%
echo.
set /p "CHOICE=Press Enter to confirm, or type a different path: "
if not "%CHOICE%"=="" set "INSTALL_DIR=%CHOICE%"

:: Create install directory
if not exist "%INSTALL_DIR%" (
    echo Creating installation directory...
    mkdir "%INSTALL_DIR%"
    if !errorLevel! neq 0 (
        echo ERROR: Failed to create directory.
        pause
        exit /b 1
    )
)

:: Copy files
echo.
echo Installing files...

:: Copy DLL
if exist "XR_APILAYER_NOVENDOR_monoeye.dll" (
    copy /Y "XR_APILAYER_NOVENDOR_monoeye.dll" "%INSTALL_DIR%\"
    echo   - DLL installed
) else (
    echo   WARNING: XR_APILAYER_NOVENDOR_monoeye.dll not found in current directory
)

:: Copy JSON manifest
if exist "XR_APILAYER_NOVENDOR_monoeye.json" (
    copy /Y "XR_APILAYER_NOVENDOR_monoeye.json" "%INSTALL_DIR%\"
    echo   - Manifest installed
) else (
    echo   WARNING: XR_APILAYER_NOVENDOR_monoeye.json not found in current directory
)

:: Copy switcher
if exist "MonoEyeSwitcher.exe" (
    copy /Y "MonoEyeSwitcher.exe" "%INSTALL_DIR%\"
    echo   - Switcher installed
)

:: Create desktop shortcut
echo.
echo Creating desktop shortcut...
set "DESKTOP=%PUBLIC%\Desktop"
if not exist "%DESKTOP%" set "DESKTOP=%USERPROFILE%\Desktop"

(
echo Set WshShell = CreateObject^("WScript.Shell"^)
echo Set oLink = WshShell.CreateShortcut^("%DESKTOP%\MonoEye Switcher.lnk"^)
echo oLink.TargetPath = "%INSTALL_DIR%\MonoEyeSwitcher.exe"
echo oLink.WorkingDirectory = "%INSTALL_DIR%"
echo oLink.Description = "MonoEye VR Rendering Switcher"
echo oLink.Save
) > "%TEMP%\create_link.vbs"

cscript //nologo "%TEMP%\create_link.vbs"
del "%TEMP%\create_link.vbs"

echo   - Desktop shortcut created

:: Enable MonoEye by default
echo.
echo Enabling MonoEye...
setx MONOEYE_ENABLE 1 /M >nul
echo   - MONOEYE_ENABLE=1 set

:: Register OpenXR API Layer in Registry
echo Registering OpenXR API Layer...
reg add "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "%INSTALL_DIR%\XR_APILAYER_NOVENDOR_monoeye.json" /t REG_DWORD /d 0 /f >nul
if %errorLevel% neq 0 (
    echo   WARNING: Failed to register OpenXR layer in HKLM. Trying HKCU...
    reg add "HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /v "%INSTALL_DIR%\XR_APILAYER_NOVENDOR_monoeye.json" /t REG_DWORD /d 0 /f >nul
)
echo   - OpenXR API Layer registered

:: Add to PATH if not already there
echo "%PATH%" | find /i "%INSTALL_DIR%" >nul
if %errorLevel% neq 0 (
    echo Adding to system PATH...
    for /f "tokens=2* delims= " %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "OLD_PATH=%%B"
    setx Path "%OLD_PATH%;%INSTALL_DIR%" /M >nul
    echo   - Added to system PATH
) else (
    echo   - Already in system PATH
)

echo.
echo ========================================
echo   Installation Complete!
echo ========================================
echo.
echo MonoEye has been installed to: %INSTALL_DIR%
echo.
echo The MonoEye Switcher shortcut has been placed on your desktop.
echo.
echo To enable/disable MonoEye for games:
echo   1. Run "MonoEye Switcher" from your desktop
echo   2. Click "Enable" or "Disable"
echo   3. Restart your VR game
echo.
echo For per-game configuration, set environment variables
echo before launching the game (see README.md).
echo.
pause
