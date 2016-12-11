/****************************************************************************
* nanotimer - The portable nanosecond-resolution timer
*
*   nanotimer.c : portable nanosecond-resolution timer
*
*****************************************************************************
* Copyright (c) 2016 by Alexander M. Albertian, <assa@4ip.ru>.
****************************************************************************/

/*#pragma offload_attribute( push, target( mic ) )
*/

#include "nanotimer.h"


#ifdef _WIN32

# ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC	1
# endif


__forceinline int
clock_gettime( int id, struct timespec *tv )
{
	static int initialized = 0;
	static LARGE_INTEGER offset;
	static ULONGLONG freq;
	static BOOL perf = 0;
	FILETIME f;
	LARGE_INTEGER t;
	ULONGLONG ut;

	( void )id;

	if ( !initialized )
	{
		LARGE_INTEGER performanceFrequency;

		if ( perf = QueryPerformanceFrequency( &performanceFrequency ) )
		{
			QueryPerformanceCounter( &offset );
			freq = performanceFrequency.QuadPart;
		}
		else
		{
			SYSTEMTIME s;

			s.wYear = 1970;
			s.wMonth = 1;
			s.wDay = 1;
			s.wHour = 0;
			s.wMinute = 0;
			s.wSecond = 0;
			s.wMilliseconds = 0;
			SystemTimeToFileTime( &s, &f );
			offset.QuadPart = ( LONGLONG )f.dwHighDateTime << 32 | f.dwLowDateTime;
			freq = hnsec_multiplier;
		}
		initialized = 1;
	}
	if ( perf )
		QueryPerformanceCounter( &t );
	else
	{
		GetSystemTimeAsFileTime( &f );
		t.QuadPart = ( LONGLONG )f.dwHighDateTime << 32 | f.dwLowDateTime;
	}
	ut = t.QuadPart - offset.QuadPart;
	tv->tv_sec = ( time_t )( ut / freq );
	tv->tv_nsec = ( long )( ut % freq * nsec_multiplier / freq );
	return 0;
}

#endif		/* _WIN32 */

/*
* nanosecond-resolution timer
*/
__declspec( noinline ) uint64_t
nanotimer( int start )
{
	static struct timespec start_time;
	uint64_t elapsed = 0;

	if ( start )
		clock_gettime( CLOCK_MONOTONIC/*CLOCK_PROCESS_CPUTIME_ID*/, &start_time );
	else
	{
		/*static */struct timespec curr_time;

		clock_gettime( CLOCK_MONOTONIC/*CLOCK_PROCESS_CPUTIME_ID*/, &curr_time );
		elapsed = ( uint64_t )( curr_time.tv_sec - start_time.tv_sec ) * nsec_multiplier +
			( uint64_t )curr_time.tv_nsec - ( uint64_t )start_time.tv_nsec;
	}
	return elapsed;
}

/*#pragma offload_attribute( pop )	// offload_attribute( target( mic ) )
*/
