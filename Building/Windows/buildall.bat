@echo off
setlocal
:: todo add help / descriptions

::: This script assumes that it resides OSCAR-data/Building/Windows
::: You must defined the path to QT configuration or passit as a parameter
::: The version is detected based on the path
::: The compiler to use is based on the qt verson.
::: @echo off
::: CONFIGURATION SECTION

set QtPath=C:\Qt

::: END CONFIGURATION SECTION
:::============================================
:::
::: START INITIALIZATIO SECTION
echo Command line: %0 %*
call :parse %*
if NOT %ERRORLEVEL%==0 goto :endProgram

set buildErrorLevel=0

::: basedir is OSCAR-code folder - contains folders Building , oscar , ...
::: parentdir is the folder that contains OSCAR-code.
::: change relative paths into absolute paths
::: Get path to this script. then cd parent folder. %cd% retunrs absolute pathname.
cd %~dp0  & cd ../../../
set parentdir=%cd%
set basedir=%parentdir%\OSCAR-code
IF NOT "%basedir%"=="%basedir: =%" (
	call :configError absolute path of OSCAR-code contains a space - not handled by Qt.
	echo %basedir%
	goto :endProgram)
IF NOT exist "%basedir%\Building" 	(
	call :notfound2 OSCAR-code: "%basedir%"
	goto :endProgram)
set baseBuildName=%basedir%\build-oscar-win
set OSCARPRO=%basedir%\oscar\oscar.pro
set requiredOutSideOscar=
IF %doOutSide%==1 (
	set baseBuildName=%parentdir%\build-oscar-win
	set OSCARPRO=%basedir%\OSCAR_QT.pro
	set requiredOutSideOscar=oscar\
	)
::: Construct name of our build directory
::: Build Directory can be based on the directories the QtCreator uses for either OSCAR_QT.pro or oscar.pro
::: For OSCAR_QT.pro  is at parentDir\OSCAR-code\OCASR_QT.pro the build will be at  parentDir\BuildDir
::: For oscar.pro  is at OSCAR-code\oscar\oscar.pro the build will be at  OSCAR-code\BuildDir

:: check if QtPath from parameters list
set defaultQtPath=%QtPath%
if NOT "%requestQtPath%"=="" (set QtPath=%requestQtPath%)

:: check valid qt folder to find version and tool folders
IF NOT exist %QtPath%\nul (
	call :configError InvalidPath to Qt. QtPath:%QtPath%
	echo The QtPath can be added as a parameter to %~nx0
	call :helpInfo
	goto :endProgram
	)
	
set foundQtVersion=
set foundQtToolsDir=
set foundQtVersionDir=

for /d %%i in (%QtPath%\*) do (call :parseQtVersion %%i)
if  "%foundQtVersion%"==""  (
	echo :configError %QtPath% is not a valid Qt folder. Must contain version 5.xx.xx or 6.xx.xx folder
	echo Please enter a valid Qt Path as a parameter
	call :helpInfo
	goto :endProgram
	)
if  "%foundQtToolsDir%"=="" (
.	echo :configError %QtPath% is not a valid Qt folder. Must contain a Tools folder
	echo Please enter a valid Qt Path as a parameter
	call :helpInfo
	goto :endProgram
	)	
echo Using QtVersion %foundQtVersion%
set qtVersion=%foundQtVersion%
set QtVersionDir=%QtPath%\%qtVersion%
set QtToolsDir=%QtPath%\Tools

::: Compiler Tools for creating Makefile (linux qmake features)
if "5.15."=="%qtVersion:~0,5%" (	
	:: For qt release 5.15
	set mingwQmakeVersion=mingw81
	set mingwQmakeTools=mingw810
) else (
	:: For qt previous releases
	set mingwQmakeVersion=mingw73
	set mingwQmakeTools=mingw730
)
::: END INITIALIZATIO SECTION


:::============================================
:main
::: Start BUILDING ON WINDOWS

call :startTimer   "Start  Time is"

if %do32%==1 (
	call :buildone 32	|| goto :finished
	if %doBrokenGl%==1 (
		call :elapseTime   "Elapse Time is"
		call :buildone 32 brokengl		|| goto :finished
	)
)


if %do64%==1 (
	if %do32%==1 (call :elapseTime   "Elapse Time is")
	call :buildone 64 	|| goto :finished
	if %doBrokenGl%==1 (
		call :elapseTime   "Elapse Time is"
		call :buildone 64 brokengl		|| goto :finished
	)
)

	
:finished

:: this does work. dir %baseBuildName%*_bit\Installer\OSCAR*.exe
echo Start  Time is %saveStartTime%
call :endTimer   "End    Time is"

FOR /D %%j in ("%baseBuildName%*") do (call :sub1 %%j)
goto :endProgram
::: return to caller
:endProgram
pause
exit /b



:::============================================
:sub1
	FOR /F %%i in ("%1\%requiredOutSideOscar%Installer\") do (call :sub2  %%i )
	FOR /F %%i in ("%1\%requiredOutSideOscar%Release\") do (call :sub2  %%i  	)
goto :eof
:sub2
::	echo PLACE 2 %1
	FOR  %%k in ("%1*.exe") do (call :sub3 %%k  	)
goto :eof
:sub3
	echo %~t1  %1
goto :eof

:parseQtVersion
	set tmpf=%~nx1
	if "5."=="%tmpf:~0,2%"	call :gotVersion %1 %tmpf% & goto :eof
	if "6."=="%tmpf:~0,2%"	call :gotVersion %1 %tmpf% & goto :eof
	if "Tools"=="%tmpf%"	call :gotTools %1 %tmpf% & goto :eof
	goto :eof
:gotTools
::	if NOT "%foundQtToolsDir%"=="" 	(echo Duplicate Qt Versions %1 and %foundQtToolsDir% Error & exit /b 101)
	set foundQtToolsDir=%1
::	echo Found QtTools %1
	goto :eof
:gotVersion
::	if NOT "%foundQtVersion%"=="" 	(echo Duplicate Qt Versions %1 and %foundQtVersion% Error & exit /b 101)
	set foundQtVersion=%2
	set foundQtVersionDir=%1
::	echo Found QtVersion %foundQtVersion%
	goto :eof

:::============================================
:parse
set do32=0
set do64=0
set doBrokenGl=0	
set doOutSide=1
set useExistingBuild=0
set skipCompile=0
set skipDeploy=0
set requestQtPath=
set toggleEcho="off"
set skipInstallCommand=
:parseLoop
IF "%1"=="" GOTO :exitParseLoop
set arg=%1
::: echo ^<%arg%^>
IF "%arg%"=="32"   (set do32=1 & GOTO :endLoop )
IF "%arg%"=="64"   (set do64=1 & GOTO :endLoop )
IF /I "%arg:~0,2%"=="br"   (set doBrokenGl=1 & GOTO :endLoop )
IF /I "%arg:~0,5%"=="SKIPD" (set skipDeploy=1 & GOTO :endLoop )
IF /I "%arg:~0,5%"=="SKIPC" (
	set useExistingBuild=1
	set skipCompile=1
	GOTO :endLoop )
IF /I "%arg:~0,2%"=="OU" (set doOutSide=1 & GOTO :endLoop )
echo windows cmd.exe workaround 1>nul
IF /I "%arg:~0,5%"=="SKIPI" (
	set skipInstallCommand=skipInstall
	GOTO :endLoop )
IF /I "%arg:~0,2%"=="re" (set useExistingBuild=1 & GOTO :endLoop )
IF /I "%arg:~0,2%"=="ve" ( echo on & GOTO :endLoop )
IF exist %arg%\Tools\nul IF exist %arg%\Licenses\nul (	
	set requestQtPath=%arg%
	GOTO :endLoop)
IF NOT "%arg:~0,2%"=="he" (
	echo _
	echo Invalid argument '%arg%'
	) else (
	echo _
	)
call :helpInfo
exit /b 123
:endLoop
SHIFT
GOTO :parseLoop
:exitParseLoop
:: echo requestQtPath ^<%requestQtPath%^>
set /a sum=%do32%+%do64%
if %sum% == 0 (set do32=1 & set do64=1 )
goto :eof


:::============================================
::: Timer functions
:::	Allows personalization of timer functions.
::: defaults displaying the curr	ent time
::: Could  Use Timer 4.0 - Command Line Timer - www.Gammadyne.com - to show time it takes to compile.
:startTimer
	set saveStartTime=%TIME%
	:: timer.exe /nologo
	echo %~1 %saveStartTime%
	goto :eof
:elapseTime
	:: timer.exe /r /nologo
	echo %~1 %TIME%
	goto :eof
:endTimer
	:: timer.exe /s /et /nologo
	echo %~1 %TIME%
	goto :eof

:::===============================================================================

:configError
set buildErrorLevel=24
echo *** CONFIGURATION ERROR ***
echo %*
goto :eof

:notfound
echo *** CONFIGURATION ERROR ***
set buildErrorLevel=25
echo NotFound %*
goto :eof



:::===============================================================================
:: Subroutine to "build one" version
:buildOne
setLocal
set bitSize=%1
set brokenGL=%2
echo Building %1 %2
::: echo do not remove this line - batch bug? 1>nul:::

set QtVersionCompilerDir=%qtVersionDir%\%mingwQmakeVersion%_%bitSize%
if NOT exist %QtVersionCompilerDir%\nul 	(
	call :buildOneError 21 notfound QtCompiler: %QtVersionCompilerDir%
	goto :endBuildOne
	)

set QtToolsCompilerDir=%QtToolsDir%\%mingwQmakeTools%_%bitSize%
if NOT exist %QtToolsCompilerDir%  (
	call :buildOneError 22 notfound   QTToolCompiler: %QtToolsCompilerDir%
	goto :endBuildOne
	)

set path=%QtToolsCompilerDir%\bin;%QtVersionCompilerDir%\bin;%PATH%
:: echo ===================================
:: echo %path%
:: echo ===================================
:::goto :eof

::: Verify all configuration parameters
set savedir=%cd%

set dirname=%baseBuildName%
if "%brokenGL%"=="brokengl" (
    set dirname=%dirname%-LegacyGFX
    set extraparams=DEFINES+=BrokenGL
)
set buildDir=%dirname%_%bitSize%_bit


:: allow rebuild without removing build when a parameter is "SKIP"
if NOT exist %buildDir%\nul goto :makeBuildDir
if %useExistingBuild%==1 goto :skipBuildDir
rmdir /s /q %buildDir%
IF NOT %ERRORLEVEL%==0 (
	call :buildOneError %ERRORLEVEL% error removing Build Folder:  %buildDir%
	GOTO :endBuildOne )

:makeBuildDir
mkdir %buildDir% 		||  (
	call :buildOneError 23 mkdir %buildDir% failed %ERRORLEVEL%
	goto :endBuildOne )
echo created %buildDir%
:skipBuildDir
cd %buildDir%
if %skipCompile%==1 goto :doDeploy



if NOT exist %buildDir%\Makefile goto :createMakefile
if %useExistingBuild%==1 goto :skipMakefile
if NOT exist %OSCARPRO% {
	call :buildOneError 24 notfound oscar.pro is not found.
	goto :endBuildOne)

:createMakefile
echo Creating Oscar's Makefile	
%QtVersionCompilerDir%\bin\qmake.exe %OSCARPRO% -spec win32-g++ %extraparams% >qmake.log 2>&1 || (
	call :buildOneError 25 Failed to create Makefile part1
	type qmake.log
	goto :endBuildOne)
%QtToolsCompilerDir%/bin/mingw32-make.exe qmake_all  >>qmake.log 2>&1  || (
	call :buildOneError  26 Failed to create Makefile part2
	type qmake.log
	goto :endBuildOne)
:skipMakefile

:: Always compile build
echo Compiling Oscar
mingw32-make.exe -j8 >make.log 2>&1 || (
	call :buildOneError  27 Make Failed
	type qmake.log
	goto :endbuildOne
	)


::: echo skipDeploy: %skipDeploy%
:doDeploy
if %skipDeploy%==1 goto :endBuildOne

echo Deploying and Creating Installation Exec in %cd%
call "%~dp0\deploy.bat" %skipInstallCommand%
set buildErrorLevel=%ERRORLEVEL%


if %buildErrorLevel% == 0 	(
	echo === MAKE %bitSize%  %brokenGL% SUCCESSFUL ===
	goto :endBuildOne
) else (
	call :buildOneError %buildErrorLevel%  DEPLOY FAILED
	goto :endBuildOne
)
:buildOneError
echo *** MAKE %bitSize%  %brokenGL% FAILED ***
echo %*
exit /b %1
:endBuildOne
cd %savedir%	1>nul 2>nul
endLocal
exit /b %buildErrorLevel%
:::============================================
:helpInfo
echo _
echo The %~nx0 script file creates Release and Install 32 and 64 bit versions of Oscar.
echo The %~nx0 has parameters to configure the default options.
echo %~nx0 can be executed from the File Manager
echo Parameter can be added using a short cut and adding parameters to the shortcut's target.
echo _	
echo OPTIONS        DESCRIPTION
echo br[okenGl]     Builds the brokenGL versions of OSCAR
echo 32             Builds just 32 bit versions
echo 64             Builds just 64 bit versions
echo ^<QtPath^>       Overrides the default path (C:\Qt) to QT's installation - contains Tools, License, ... folders
echo ou[tside]      Default: Build is located outside of git repository. Same as building with OSCAR_QT.pro
echo in[side]       Build is located inside of git repository. Same as building with oscar.pro
echo he[lp]         Display this help message
echo _	
echo The offical builds of OSCAR should not use the following options. These facilate development and testing
echo re[make]       Skips recreating Makefile, but executes make in existing build folder
echo skipC[ompile]  Skips all compiling. Leaves build folder untouched.
echo skipD[eploy]   Skip calling deploy.bat. Does not create Release or install versions.
echo ve[rbose]      Start verbose logging
echo _
echo ^<Anything^>     Displays invalid parameter message and help message then exits.
goto :eof




