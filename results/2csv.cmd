@echo off
setlocal enableextensions
setlocal disabledelayedexpansion

echo *************************************************************** 2016.12.20 ** >&2
echo * DLS9 enumerator - Optimized MPI version of the DLS enumeration application  >&2
echo *                                                                             >&2
echo *   2csv.bat    : Convert benchmarking data into the CSV format               >&2
echo *                                                                             >&2
echo ***************************************************************************** >&2
echo * Copyright (c) 2016 by Alexander M. Albertian, ^<assa@4ip.ru^>.                >&2
echo *                                                                             >&2
echo * Usage:                                                                      >&2
echo *   2csv.bat [^<directory-with-log-files^>] [^<separator-character^>]             >&2
echo ***************************************************************************** >&2
echo. >&2

set "SEPARATOR=%~2"
if "%SEPARATOR%"=="" set "SEPARATOR=,"
echo "Number of processes"%SEPARATOR%"Squares per second"%SEPARATOR%"Time per square (ns)"

set "DATA=%~dpnx1"
if "%DATA%"=="" set "DATA=%CD%"
cd /d "%DATA%"

for /f "tokens=* delims=" %%i in ( 'dir /b /odsn *.log' ) do (
	call :rename "%%~i"
)

rem for /f %%i in ( 'dir /b /odsn *.log' ) do (
for /f "tokens=* delims=" %%i in ( 'dir /b /one *.log' ) do (
	call :eval "%%~i"
)
set "err_msg="
goto error

:rename
if "%~1"=="" goto :eof
setlocal
set "NAME=000%~n1"
set "EXT=%~x1"
set "NAME=%NAME:~-4%"
rem echo %NAME% >&2
if /i not "%~nx1"=="%NAME%%EXT%" ren "%~dpnx1" "%NAME%%EXT%" >nul
endlocal
goto :eof

:eval
if "%~1"=="" goto :eof
setlocal
set /a "NPROC=1%~n1-10000" 2>nul
rem echo %NPROC%: >&2
set SPS=
set TPS=
for /f "usebackq tokens=1,2 delims==#*:[]" %%i in ( "%~dpnx1" ) do (
	rem call :split SPS "%%~i" "Total Avg. squares per second" "%%~j"
	rem if errorlevel 1 call :split TPS "%%~i" "Total Avg. time per square" "%%~j"
	if /i "%%~i"=="  Total Avg. squares per second" call :strip SPS "%%~j"
	if /i "%%~i"=="     Total Avg. time per square" call :strip TPS "%%~j"
)
if not "%SPS%"=="" if not "%TPS%"=="" echo %NPROC%%SEPARATOR%%SPS%%SEPARATOR%%TPS%
endlocal
goto :eof

:strip
if "%~1"=="" goto :eof
if "%~2"=="" goto :eof
setlocal
set "_var=%~1"
set "_val=%~2"
set "_tail=%_val:~-2%"
if "%_tail%"=="ns" set "_val=%_val:~0,-2%"
set "_tail=%_tail:~-1%"
if "%_tail%"=="M" set "_val=%_val:~0,-1%"
set "_val=%_val:.=%"
set "_val=%_val:	=%"
set "_val=%_val: =%"
for /f "tokens=* delims=0" %%i in ( "%_val%" ) do set "_val=%%~i"
endlocal & call set "%_var%=%_val%"
goto :eof

:split
for %%i in ( "%~1" "%~2" "%~3" "%~4" ) do if "%%~i"=="" verify shit 2>nul & goto :eof
setlocal
for /f "tokens=* delims=	 " %%i in ( "%~2" ) do set "_name=%%~i"
if /i not "%_name%"=="%~3" endlocal & verify shit 2>nul & goto :eof
set "_var=%~1"
call :strip _val "%~4"
endlocal & call set "%_var%=%_val%" & verify >nul & goto :eof

:error
if not "%err_msg%"=="" echo *** ERROR: %err_msg% >&2 & pause
endlocal & endlocal