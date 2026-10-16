@echo off
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0serial_dashboard.ps1" %*
