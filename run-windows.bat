@echo off
cd /d "%~dp0"

where pwsh.exe >nul 2>&1
if %errorlevel% equ 0 (
    pwsh.exe -NoProfile -File ".\setup.ps1"
) else (
    powershell.exe -NoProfile -File ".\setup.ps1"
)

rmdir /s /q build
cmake --preset ninja
cmake --build --preset ninja

build\bin\main.exe