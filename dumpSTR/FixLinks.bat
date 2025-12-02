@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: Git Symlink Fixer - Converts file-based symlinks to actual Windows symlinks
:: ============================================================================

:: ============================================================================
:: Set and confirm path to files
:: ============================================================================
set dumpSTRPath=c:\oscar\OSCAR-code\dumpSTR

echo .
echo FixLinks assumes that dumpSTR is located at %dumpSTRPath%
echo .

choice /C YN /M "Is this correct?"
if %errorlevel% EQU 1 goto setPaths
echo .
echo Edit dumpSTRPath in fixlinks.bat to point to the directory containing dumpSTR files
echo Then rerun fixlinks.bat
exit /b 1

:: Define the three filenames that should be symlinks (modify these paths as needed)
:SetPaths
set "FILE1=%dumpSTRPath%\edfparser.cpp"
set "FILE2=%dumpSTRPath%\edfparser.h"
set "FILE3=%dumpSTRPath%\test_macros.h"

:: Check for administrator privileges
echo Checking for administrator privileges...
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo ERROR: This script requires Administrator privileges.
    echo.
    echo Please right-click on this batch file and select "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo Administrator privileges confirmed.
echo.

:: Process each file
call :ProcessFile "%FILE1%"
call :ProcessFile "%FILE2%"
call :ProcessFile "%FILE3%"

echo.
echo Processing complete.
pause
exit /b 0

:: ============================================================================
:: Subroutine to process a single file
:: ============================================================================
:ProcessFile
set "FILEPATH=%~1"

echo Processing: %FILEPATH%

:: Check if file exists
if not exist "%FILEPATH%" (
    echo   WARNING: File does not exist, skipping.
    echo.
    goto :eof
)

:: Check if it's already a symlink
dir "%FILEPATH%" | findstr /C:"<SYMLINK>" >nul 2>&1
if %errorLevel% equ 0 (
    echo   OK: Already a symbolic link.
    echo.
    goto :eof
)

:: It's a regular file - read the target path from it
echo   File is not a symlink, reading target path...
set "TARGETPATH="
for /f "usebackq delims=" %%a in ("%FILEPATH%") do (
    if not defined TARGETPATH set "TARGETPATH=%%a"
)

if not defined TARGETPATH (
    echo   ERROR: Could not read target path from file.
    echo.
    goto :eof
)

echo   Target path: !TARGETPATH!

:: Convert relative path if needed (assumes Unix-style forward slashes in git symlinks)
set "TARGETPATH=!TARGETPATH:/=\!"

:: Determine if target is a file or directory
set "LINKTYPE=/D"
if exist "!TARGETPATH!\*" (
    set "LINKTYPE=/D"
    echo   Target is a directory.
) else if exist "!TARGETPATH!" (
    set "LINKTYPE="
    echo   Target is a file.
) else (
    echo   WARNING: Target path does not exist: !TARGETPATH!
    echo   Creating symlink anyway...
)

:: Delete the regular file
echo   Deleting regular file...
del /f /q "%FILEPATH%"
if %errorLevel% neq 0 (
    echo   ERROR: Could not delete file.
    echo.
    goto :eof
)

:: Create the symbolic link
echo   Creating symbolic link...
if defined LINKTYPE (
    mklink /D "%FILEPATH%" "!TARGETPATH!"
) else (
    mklink "%FILEPATH%" "!TARGETPATH!"
)

if %errorLevel% equ 0 (
    echo   SUCCESS: Symbolic link created.
) else (
    echo   ERROR: Failed to create symbolic link.
)
echo.
goto :eof