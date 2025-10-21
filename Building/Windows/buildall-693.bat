setlocal
:::@echo off
::: You must set these paths to your QT configuration
set qtpath=C:\Qt
set qtVersion=6.9.3
set minver=1310

::: This file has been updated to work with Qt 6.9.3 and mingw1310_64
:::
::: Build 64-bit version of OSCAR for Windows.
::: Code to support 32-bin Windows and code to build BrokenGL (LegacyGFX) has been removed
::: Uses Timer 4.0 - Command Line Timer - www.Gammadyne.com - to show time it takes to compile.  This could be removed.

setlocal
::: timer /nologo
set QTDIR=%qtpath%\%qtversion%\mingw_64
echo QTDIR is %qtdir%

set PATH=%qtpath%\Tools\mingw1310_64\bin;%qtdir%\bin;%PATH%
::: echo %PATH%

set savedir=%cd%

: Construct name of our build directory
set dirname=build-oscar-win_64_bit
echo Build directory is %dirname%

set basedir=..\..
if exist %basedir%\%dirname%\nul rmdir /s /q %basedir%\%dirname%
mkdir %basedir%\%dirname%
cd %basedir%\%dirname%

::: timer /nologo
%qtpath%\%qtVersion%\mingw_64\bin\qmake.exe ..\oscar\oscar.pro -spec win32-g++ %extraparams% >qmake.log 2>&1 && %qtpath%\Tools\mingw1310_64\bin\mingw32-make.exe qmake_all >>qmake.log 2>&1 
%qtpath%\Tools\mingw1310_64\bin\mingw32-make.exe -j12 >make.log 2>&1 || goto :makefail
::: timer /s /nologo
  
::: timer /nologo
call ..\Building\Windows\deploy.bat
::: timer /s /nologo

::: timer /s /nologo
echo === MAKE  SUCCESSFUL ===
cd %savedir%
endlocal
exit /b

:makefail
endlocal
::: timer /s /nologo
echo *** MAKE  FAILED ***
pause
exit /b