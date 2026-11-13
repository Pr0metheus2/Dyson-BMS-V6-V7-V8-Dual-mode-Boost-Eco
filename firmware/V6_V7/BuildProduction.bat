@echo off
setlocal
cd /d "%~dp0"

for /f "tokens=3" %%V in ('findstr /R /C:"^[ ]*#define[ ]*FIRMWARE_VERSION[ ]*[0-9][0-9]*\.[0-9][0-9]*" config.h') do set "FIRMWARE_VERSION=%%V"
set "FIRMWARE_VERSION=%FIRMWARE_VERSION:;=%"
if not defined FIRMWARE_VERSION (
    echo Could not read FIRMWARE_VERSION from config.h
    exit /b 1
)

set "MPLABX_ROOT="
set "DFP_DIR="
if exist "C:\Program Files\Microchip\MPLABX\v6.20\gnuBins\GnuWin32\bin\make.exe" (
    set "MPLABX_ROOT=C:\Program Files\Microchip\MPLABX\v6.20"
    set "DFP_DIR=C:\Program Files\Microchip\MPLABX\v6.20\packs\Microchip\PIC12-16F1xxx_DFP\1.7.242"
) else if exist "C:\Program Files\Microchip\MPLABX\v6.30\gnuBins\GnuWin32\bin\make.exe" (
    set "MPLABX_ROOT=C:\Program Files\Microchip\MPLABX\v6.30"
    set "DFP_DIR=C:\Program Files\Microchip\MPLABX\v6.30\packs\Microchip\PIC12-16F1xxx_DFP\1.8.254"
) else (
    echo MPLAB X 6.20 or 6.30 was not found.
    exit /b 1
)

if not exist "%DFP_DIR%\xc8" (
    echo Required PIC12-16F1xxx device pack was not found: %DFP_DIR%
    exit /b 1
)

"%MPLABX_ROOT%\gnuBins\GnuWin32\bin\make.exe" -f Makefile CONF=default build "DFP_DIR=%DFP_DIR%"

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

copy /y "dist\default\production\V6_V7.production.hex" "dist\default\production\V6_V7.production_%FIRMWARE_VERSION%.hex" >nul
if errorlevel 1 (
    echo Could not create versioned HEX file
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
echo HEX: %CD%\dist\default\production\V6_V7.production_%FIRMWARE_VERSION%.hex

@pause
