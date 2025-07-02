setlocal
:::@echo off
::: You must set these paths to your QT configuration
set qtpath=C:\Qt
set qtVersion=5.15.2

::: This file has been updated to work with Qt 5.15.2 and mingw 8.1.0
:::
::: Build 32- and 64-bit versions of OSCAR for Windows.
::: Includes code to build BrokenGL (LegacyGFX) versions, but that option is not currently used
::: Uses Timer 4.0 - Command Line Timer - www.Gammadyne.com - to show time it takes to compile.  This could be removed.
::: timer /nologo

:::call :buildone 32 brokengl

call :buildone 64

call :buildone 32

:::call :buildone 64 brokengl
::: timer /s /nologo
goto :eof

::: Subroutine to build one version
:buildone
setlocal
::: timer /nologo

set QTDIR=%qtpath%\%qtversion%\mingw81_%1
echo QTDIR is %qtdir%

set PATH=%qtpath%\Tools\mingw810_%1\bin;%qtpath%\%qtVersion%\mingw81_%1\bin;%PATH%
::: echo %PATH%

set savedir=%cd%

: Construct name of our build directory
set dirname=build-oscar-win_%1_bit
if "%2"=="brokengl" (
    set dirname=%dirname%-LegacyGFX
    set extraparams=DEFINES+=BrokenGL
)
echo Build directory is %dirname%

set basedir=..\..
if exist %basedir%\%dirname%\nul rmdir /s /q %basedir%\%dirname%
mkdir %basedir%\%dirname%
cd %basedir%\%dirname%

%qtpath%\%qtVersion%\mingw81_%1\bin\qmake.exe ..\oscar\oscar.pro -spec win32-g++ %extraparams% >qmake.log 2>&1 && %qtpath%\Tools\mingw810_%1\bin\mingw32-make.exe qmake_all >>qmake.log 2>&1 
%qtpath%\Tools\mingw810_%1\bin\mingw32-make.exe -j8 >make.log 2>&1 || goto :makefail
  
call ..\Building\Windows\deploy.bat

::: timer /s /nologo
echo === MAKE %1  %2 SUCCESSFUL ===
cd %savedir%
endlocal
exit /b

:makefail
endlocal
::: timer /s /nologo
echo *** MAKE %1  %2 FAILED ***
pause
exit /b