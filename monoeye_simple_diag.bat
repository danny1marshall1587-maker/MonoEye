@echo off
echo MonoEye Diagnostic Test
echo.
echo [1] Checking Registry:
reg query "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /s 2>nul | findstr /i "monoeye"
echo.
echo [2] Checking Environment:
set MONOEYE
echo.
echo [3] Checking Log File:
dir "%USERPROFILE%\Documents\MonoEye\monoeye.log"
echo.
echo [4] Recent Log Content:
type "%USERPROFILE%\Documents\MonoEye\monoeye.log"
echo.
echo Done.
pause
