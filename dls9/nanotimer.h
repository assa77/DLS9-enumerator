/****************************************************************************
* nanotimer - The portable nanosecond-resolution timer
*
*   nanotimer.h : portable nanosecond-resolution timer header
*
*****************************************************************************
* Copyright (c) 2016 by Alexander M. Albertian, <assa@4ip.ru>.
****************************************************************************/

#ifndef __NANOTIMER_H_INCLUDED
#define __NANOTIMER_H_INCLUDED


/*#define _GNU_SOURCE*/

#include "stdafx.h"
/*#include "types.h"*/

#ifdef _WIN32
/*# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>*/
#endif		/* !_WIN32 */

/*#include <time.h>*/
/*#include <stdint.h>*/


#ifdef __cplusplus
extern "C" {
#endif

static const uint64_t nsec_multiplier	= 1000000000ul;

#ifdef _WIN32
static const uint64_t hnsec_multiplier	= 10000000ul;


int
clock_gettime( int, struct timespec *tv );
#endif		/* _WIN32 */

/*
* nanosecond-resolution timer
*/
uint64_t
nanotimer( int start );

#ifdef __cplusplus
}
#endif


#endif		/* __NANOTIMER_H_INCLUDED */
