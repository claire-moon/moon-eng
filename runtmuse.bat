@echo off
cls
echo ==================================
echo     T M U S E  L A U N C H E R
echo ==================================
echo.
echo 1. run TMUSE (TUI)
echo 2. run TMUSE (GUI)
echo 3. Exit to DOS
echo.

choice /c:123 /n "SELECT AN OPTION [1-3]..."

if errorlevel 3 goto end
if errorlevel 2 goto gui
if errorlevel 1 goto tui

:tui
tmuse.exe
goto end

:gui
tmusegui.exe
goto end

:end
