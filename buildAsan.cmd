@echo off
setlocal
TASKKILL /IM OBS64.exe
powershell.exe -ExecutionPolicy Bypass -File "%~dp0run-obs.ps1" -Config RelWithDebInfo -Asan
exit /b %ERRORLEVEL%