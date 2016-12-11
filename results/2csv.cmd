@echo off
setlocal enableextensions
setlocal disabledelayedexpansion

echo ***************************************************************************** >&2
echo * DLS9 enumerator - Optimized MPI version of the DLS enumeration application  >&2
echo *                                                                             >&2
echo *   2csv.bat    : Convert benchmarking data to CSV                            >&2
echo *                                                                             >&2
echo ***************************************************************************** >&2
echo * Copyright (c) 2016 by Alexander M. Albertian, ^<assa@4ip.ru^>.                >&2
echo *                                                                             >&2
echo * Usage:                                                                      >&2
echo *   2csv.bat [^<directory-with-log-files^>]                                     >&2
echo ***************************************************************************** >&2
echo. >&2

set "SEPARATOR=;"

set "DATA=%~dpnx1"
if "%DATA%"=="" set "DATA=%CD%"

cd /d "%DATA%"

echo "Number of processes"%SEPARATOR%"Squares per second"%SEPARATOR%"Time per square (ns)"

for /f "tokens=* delims=" %%i in ( 'dir /b /odsn *.log' ) do (
	call :rename "%%~i"
)

rem for /f %%i in ( 'dir /b /odsn *.log' ) do (
for /f "tokens=* delims=" %%i in ( 'dir /b /one *.log' ) do (
	call :eval "%%~i"
)
set "err_msg="

:error
if not "%err_msg%"=="" echo *** ERROR: %err_msg% >&2 & pause
endlocal & endlocal & goto :eof

:rename
if "%~1"=="" goto :eof
setlocal
set "NAME=000%~n1"
set "EXT=%~x1"
set "NAME=%NAME:~-4%"
rem echo %NAME% >&2
if /i not "%~dpnx1"=="%NAME%%EXT%" ren "%~dpnx1" "%NAME%%EXT%" >nul
endlocal
goto :eof

:eval
if "%~1"=="" goto :eof
setlocal
set /a "NPROC=1%~n1-10000" 2>nul
rem echo %NPROC%: >&2
set SQR=
set SPS=
set TPS=
for /f "usebackq tokens=1,2 delims==#*:[]" %%i in ( "%~dpnx1" ) do (
	rem if /i "%%~i"=="            Total Squares found" set "SQR=%%~j"
	if /i "%%~i"=="  Total Avg. squares per second" set "SPS=%%~j"
	if /i "%%~i"=="     Total Avg. time per square" set "TPS=%%~j"
)
rem call :strip SQR "%SQR%"
call :strip SPS "%SPS%"
call :strip TPS "%TPS%"
echo %NPROC%%SEPARATOR%%SPS%%SEPARATOR%%TPS%
endlocal
goto :eof

:strip
if "%~1"=="" goto :eof
if "%~2"=="" goto :eof
setlocal
set "var=%~1"
set "val=%~2"
set "val=%val:ns=%"
set "val=%val:M=%"
set "val=%val:.=%"
set "val=%val: =%"
endlocal & call set "%var%=%val%"
goto :eof