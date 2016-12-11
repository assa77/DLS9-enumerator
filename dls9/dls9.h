/****************************************************************************
* DLS9 enumerator - Optimized MPI version of the DLS enumeration application
*
*   dls9.h      : DLS enumerator header
*
*****************************************************************************
* Based on Nauchnik (Oleg Zaikin) sources:
*   <https://github.com/Nauchnik/DLS9_enumeration>
*
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

#ifndef __DLS9_H_INCLUDED
#define __DLS9_H_INCLUDED


#include "stdafx.h"

#ifdef _WIN32
//# ifndef WIN32_LEAN_AND_MEAN
//#  define WIN32_LEAN_AND_MEAN
//# endif
//# include <windows.h>
# define YIELD( ) SwitchToThread( )
//# define YIELD( ) Sleep( 0 )
#else		// !_WIN32
//# define _GNU_SOURCE
//# include <unistd.h>
# define YIELD( ) sched_yield( )
#endif		// _WIN32

//#include <mpi.h>
//#include <string>
//#include <ios>
//#include <iostream>
//#include <fstream>
//#include <sstream>
//#include <vector>
//#include <climits>
//#include <cerrno>
//#include <cstdlib>

#include "nanotimer.h"


#define CACHE_ALIGN		64

//
// EXPLICIT_LOOP_CONTER:	Enable inner loop unrolling/vectorizing
//#ifdef __INTEL_OFFLOAD
#ifndef __MIC__
# define EXPLICIT_LOOP_CONTER	1
#endif


namespace dls9
{

//using namespace std;
using std::size_t;
using std::ptrdiff_t;

using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;


enum wu_state {
	NOT_STARTED = -1,
	IN_PROGRESS = 0,
	SOLVED = 1
};


static const size_t log_line	= 32;
static const char log_space	= ' ';
static const char log_star	= '*';
static const char log_quote	= '"';

#ifdef _WIN32
static const char s_name[ ]	= "dls9.exe";
static const char s_keys[ ]	= "-/";
#else		// !_WIN32
# ifdef __MIC__
static const char s_name[ ]	= "./dls9.mic";
# else		// !__MIC__
static const char s_name[ ]	= "./dls9";
# endif		// __MIC__
static const char s_keys[ ]	= "-";
#endif		// _WIN32
static const char s_spaces[ ]	= " \t";


static const uint64_t log_dly	= 10 * nsec_multiplier;

static const uint64_t mega	= 1000000ul;
static const uint64_t megax	= 100 * mega;
static const size_t alignx	= CACHE_ALIGN;

static const size_t N		= 9;
static const size_t NN		= N * N;
static const size_t maxN	= static_cast< size_t >( 1 ) << N;
static const size_t AllN	= maxN - 1;


//
// nsec: print time in seconds
//
class nsec
{
	/*static */uint64_t in_nsec;

	//
	// disable copy Ctor and assignment operator
	//
	nsec( const nsec & )/* = delete*/;

	void /*nsec &*/operator = ( const nsec & )/* = delete*/;

public:
	//
	// Ctor with time specified in ns
	//
	/*explicit */nsec( uint64_t _ns = 0 ) :
		in_nsec( _ns )
	{
	}

	//
	// Dtor
	//
	~nsec( )
	{
	}

	//
	// print time in seconds
	//
	friend std::ostream &
	operator << ( std::ostream &_os, const nsec &_nsec );
};	// class nsec

std::ostream &
operator << ( std::ostream &_os, const nsec &_nsec )
{
	_os.clear( );
//	_os.flush( );
//	_os.clear( );
	_os << _nsec.in_nsec / nsec_multiplier << '.' << std::setfill( '0' ) << std::setw( 9 ) << _nsec.in_nsec % nsec_multiplier << std::setfill( log_space );
//	if ( !_os.good( ) ? _os.clear( ), _os.flush( ), true : _os.flush( ), false )
//		throw std::runtime_error( string( IsLocation"Error writing stream" ) );
	return _os;
}


//
// wu: work unit descriptor
//
__declspec( align( alignx ) ) struct wu
{
	std::string	first_cells_known_values;
	long long	dls_number;
	uint64_t	start_time;
//	double		start_processing_time;
//	double		end_procesing_time;
	double		processing_time;
	wu_state	state;
};

};		// namespace dls9


#endif		// __DLS9_H_INCLUDED
