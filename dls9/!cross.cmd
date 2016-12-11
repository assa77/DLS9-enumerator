@echo off
setlocal enableextensions
setlocal disabledelayedexpansion

echo *****************************************************************************
echo * DLS9 enumerator - Optimized MPI version of the DLS enumeration application
echo *
echo *   !cross.bat  : CMD batch file for MIC cross-compilation
echo *
echo *****************************************************************************
echo * Copyright (c) 2016 by Alexander M. Albertian, ^<assa@4ip.ru^>.
echo *****************************************************************************
echo.

rem call iclvars.bat intel64

set "prj_name=dls9"

set "err_msg=NUMBER_OF_PROCESSORS environment variable isn't set"
if "%NUMBER_OF_PROCESSORS%"=="" goto :error

set "err_msg=Intel C/C++ compiler isn't found: icl.exe"
"%SystemRoot%\system32\where.exe" icl.exe >nul 2>nul
if errorlevel 1 goto :error

set "err_msg=Intel MPI Library for MIC isn't found"
if not exist "%I_MPI_ROOT%\mic\include\mpi.h" goto :error

set "err_msg=No source file(s)"
if not exist "%prj_name%.cpp" goto :error
if not exist "nanotimer.c" goto :error

set "err_msg=Creation of %prj_name%.mic failed..."
if exist %prj_name%.mic del /q /f %prj_name%.mic >nul 2>nul
rem icl.exe /Qmic -mmic -m64 -w3 -wd161,981,2547 -nologo -O3 -no-prec-div -fp-model fast=2 -std=c++11 -Zp16 -align -ipo -multiple-processes=%NUMBER_OF_PROCESSORS% -qopt-report=5 -qopt-report-phase=all -qopt-report-embed -fno-fnalias -fno-exceptions -fargument-noalias -alias-const -ansi-alias -qopt-matmul -use-intel-optimized-headers -qopenmp -parallel -par-threshold=50 -vec-threshold=50 -qopt-class-analysis -unroll -unroll-aggressive -qopt-prefetch=3 -qopt-streaming-stores=auto -qopt-multi-version-aggressive -no-inline-min-size -no-inline-max-size -no-inline-max-total-size -inline-factor=1000 -I "%I_MPI_ROOT%\mic\include" -static-intel -no-intel-extensions %prj_name%.cpp nanotimer.c -lmpi -lmpicxx -lrt -o %prj_name%.mic 2>&1 |more
icl.exe /Qmic -mmic -m64 -w3 -wd161,981,2547 -nologo -O3 -no-prec-div -fp-model fast=2 -std=c++11 -Zp16 -align -ipo -multiple-processes=%NUMBER_OF_PROCESSORS% -qopt-report=5 -qopt-report-phase=all -qopt-report-embed -fno-fnalias -fno-exceptions -fargument-noalias -alias-const -ansi-alias -qopt-matmul -use-intel-optimized-headers -par-threshold=50 -vec-threshold=50 -qopt-class-analysis -unroll -unroll-aggressive -qopt-prefetch=3 -qopt-streaming-stores=auto -qopt-multi-version-aggressive -no-inline-min-size -no-inline-max-size -no-inline-max-total-size -inline-factor=1000 -I "%I_MPI_ROOT%\mic\include" -static-intel -no-intel-extensions %prj_name%.cpp nanotimer.c -lmpi -lmpicxx -lrt -lpthread -o %prj_name%.mic 2>&1 |more
if errorlevel 1 goto error
if not exist %prj_name%.mic goto error
echo *** File %prj_name%.mic has been successfully created!
set "err_msg="

:error
if not "%err_msg%"=="" echo *** ERROR: %err_msg% >&2 & pause
endlocal & endlocal & goto :eof