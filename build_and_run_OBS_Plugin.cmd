@echo off
setlocal
TASKKILL /IM OBS64.exe
REM Run PowerShell script from this folder
powershell.exe -ExecutionPolicy Bypass -File "%~dp0run-obs.ps1" -Config RelWithDebInfo
exit /b %ERRORLEVEL%