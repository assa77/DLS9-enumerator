@echo off
setlocal enableextensions
setlocal disabledelayedexpansion

echo *****************************************************************************
echo * DLS9 enumerator - Optimized MPI version of the DLS enumeration application
echo *
echo *   !bench.bat  : CMD benchmarking script
echo *
echo *****************************************************************************
echo * Copyright (c) 2016 by Alexander M. Albertian, ^<assa@4ip.ru^>.
echo *
echo * Usage:
echo *   !bench.bat [^<minimal-number-of-ranks^> [^<maximal-number-of-ranks^>]]
echo *****************************************************************************
echo.

set "EXE=..\x64\Release\dls9.exe"
set "DATA=dls9_count_data"

for %%i in ( "%EXE%" "%DATA%" ) do (
	if not exist "%%~dpnxi" (
		set err_msg=File "%%~dpnxi" doesn't exist
		goto error
	)
)

set MPIEXEC=
for /f "tokens=* delims=" %%f in ( '"%SystemRoot%\system32\where.exe" mpiexec.exe' ) do (
	set "MPIEXEC=%%~dpnxf"
	goto mpiok
)
:mpiok

set "err_msg=Can't find mpiexec.exe"
if not exist "%MPIEXEC%" goto error

echo.MPIEXEC : "%MPIEXEC%" >&2

set START=1
set STEP=1

set /a "NPROC=%~1" 2>nul
set /a "STOP=%~2" 2>nul

if not "%NPROC%"=="" set /a "START=%NPROC%" 2>nul
set /a "START+=1"

set "err_msg=NUMBER_OF_PROCESSORS environment variable isn't set"
if "%STOP%"=="" (
	if "%NUMBER_OF_PROCESSORS%"=="" goto error
	set /a "STOP=%NUMBER_OF_PROCESSORS%*3/2"
)
set /a "STOP+=1"

echo.  START = %START% >&2
echo.   STOP = %STOP% >&2
echo.   STEP = %STEP% >&2
echo.

set "err_msg=Invalid number of processes"
if %START% lss 2 goto error
if %START% gtr %STOP% goto error

for /l %%i in ( %START%, %STEP%, %STOP% ) do (
	echo.= %%i Ranks ============================= >&2

	rem mpiexec.exe -np %%i "%EXE%" "%DATA%" -1 nul: >"%%i.log" 2>&1
	mpiexec.exe -np %%i "%EXE%" "%DATA%" -2 nul: >"%%i.log" 2>&1
	if errorlevel 1 (
		set "err_msg=(%errorlevel%)"
		goto error
	)

	echo....Ok >&2
)
set "err_msg="

:error
if not "%err_msg%"=="" echo *** ERROR: %err_msg% >&2 & pause
endlocal & endlocal & goto :eof