@echo off
setlocal
cd /d "%~dp0"

"C:\Program Files\Microchip\MPLABX\v6.20\gnuBins\GnuWin32\bin\make.exe" -f Makefile CONF=default build

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
echo HEX: %CD%\dist\default\production\V8.production.hex

@pause