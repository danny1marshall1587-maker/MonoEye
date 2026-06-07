@echo off
echo ======================================================
echo   MonoEye v0.5.31 - Git Push Helper
echo ======================================================
echo.
echo Staging all changes...
git add .
echo.
echo Committing changes...
git commit -m "Update MonoEye to v0.5.31: Universal VR Support and AC Evo Forcing Logic"
echo.
echo Pushing to GitHub...
git push
echo.
echo Done!
pause
