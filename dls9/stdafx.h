/****************************************************************************
* DLS9 enumerator - Optimized MPI version of the DLS enumeration application
*
*   stdafx.h
*
*****************************************************************************
* Copyright (c) 2016 by Alexander M. Albertian, <assa@4ip.ru>.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
//#include <tchar.h>
#else		// !_WIN32
//# define _GNU_SOURCE
# include <unistd.h>
#endif		// _WIN32

#include <stdio.h>
#include <time.h>
#ifdef __cplusplus
# include <string>
# include <cstring>
# include <ios>
# include <iostream>
# include <iomanip>
# include <fstream>
# include <sstream>
# include <algorithm>
# include <vector>
# include <cstdint>
# include <climits>
# include <cerrno>
# include <cstdlib>
#else		// !__cplusplus
# include <string.h>
# include <stdint.h>
# include <limits.h>
# include <errno.h>
# include <stdlib.h>
#endif		// __cplusplus

//#include <omp.h>
//#ifndef __MIC__
# include <mpi.h>
//#endif

// TODO: reference additional headers your program requires here
