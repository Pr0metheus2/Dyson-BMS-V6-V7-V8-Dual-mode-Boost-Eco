@echo off
setlocal
cd /d "%~dp0"

for /f "tokens=3" %%V in ('findstr /R /C:"^[ ]*#define[ ]*FIRMWARE_VERSION[ ]*[0-9][0-9]*\.[0-9][0-9]*" config.h') do set "FIRMWARE_VERSION=%%V"
set "FIRMWARE_VERSION=%FIRMWARE_VERSION:;=%"
if not defined FIRMWARE_VERSION (
    echo Could not read FIRMWARE_VERSION from config.h
    exit /b 1
)

"C:\Program Files\Microchip\MPLABX\v6.20\gnuBins\GnuWin32\bin\make.exe" -f Makefile CONF=default build

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

copy /y "dist\default\production\V8.production.hex" "dist\default\production\V8.production_%FIRMWARE_VERSION%.hex" >nul
if errorlevel 1 (
    echo Could not create versioned HEX file
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
echo HEX: %CD%\dist\default\production\V8.production_%FIRMWARE_VERSION%.hex

@pause
