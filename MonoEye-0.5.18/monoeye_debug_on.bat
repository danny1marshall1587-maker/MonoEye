@echo off
echo ========================================
echo   MONOEYE DEBUG ENABLER
echo ========================================
echo.
echo [1] Setting environment variables...
set MONOEYE_LOG_ENABLED=1
set MONOEYE_LOG_LEVEL=debug
set MONOEYE_ENABLE=1

echo.
echo [2] Done! Variables are set for this window.
echo.
echo [3] IMPORTANT: You must launch your game 
echo     launcher (Steam) from THIS window.
echo.
echo Type the path to your game or launcher 
echo and press ENTER, or just leave this 
echo window open while you test.
echo.
pause
