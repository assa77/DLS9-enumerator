/****************************************************************************
* DLS9 enumerator - Optimized MPI version of the DLS enumeration application
*
*   dls9.cpp    : DLS enumerator
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

//#pragma offload_attribute( push, target( mic ) )

//#include "stdafx.h"
#include "dls9.h"
//#include "nanotimer.h"


namespace dls9
{

__declspec( align( alignx ) ) static uint32_t LS[ NN ];
__declspec( align( alignx ) ) static uint32_t CR[ NN ];
__declspec( align( alignx ) ) static uint32_t L[ NN ];
__declspec( align( alignx ) ) static unsigned long long SquaresCnt = 0, dSquares = 0;
__declspec( align( alignx ) ) static unsigned long perf_limit = 0;
__declspec( align( alignx ) ) static uint32_t Columns[ N ], Rows[ N ], MD, AD;
__declspec( align( alignx ) ) static unsigned char FileData[ N + 2 ];// = "1203652647";


template< class T >
__declspec( noinline ) static void
Logger( std::ostream &_os, int id, char decor, T num, uint64_t ns ) noexcept
{
#ifdef __EXCEPTIONS
	try
	{
#endif
		std::string prefix;
	//	_os.clear( );
		_os.exceptions( );
		if ( id == -1 )
		{
			prefix = " Total ";
			_os << std::endl << std::setfill( decor ) << std::setw( log_line ) << "[" + prefix + "]" << std::setfill( log_space ) << std::endl;
		}
		else
			_os << std::endl << std::setfill( decor ) << std::setw( log_line ) << " #" << std::setfill( log_space ) << id << std::endl;
		_os << decor << log_space << std::setw( log_line ) << prefix + "Squares found: " << static_cast< uint64_t >( num / static_cast< T >( mega ) ) << 'M' << std::endl;
		if ( ns )
		{
			T sps = num * nsec_multiplier / ns;
			_os << decor << log_space << std::setw( log_line ) << prefix + "Avg. squares per second: " << static_cast< uint64_t >( sps / static_cast< T >( mega ) ) <<
				'.' << std::setfill( '0' ) << std::setw( 6 ) << static_cast< uint64_t >( sps ) % mega << std::setfill( log_space ) << 'M' << std::endl;
		}
		if ( num >= static_cast< T >( 1 ) )
			_os << decor << log_space << std::setw( log_line ) << prefix + "Avg. time per square: " << static_cast< uint64_t >( ns / num ) << " ns" << std::endl;
		_os << decor << log_space << std::endl;
		_os.flush( );
		_os.clear( );
#ifdef __EXCEPTIONS
	}
	catch ( ... )
	{
	//	_os.clear( );
		_os.exceptions( );
		_os.flush( );
		_os.clear( );
	}
#endif
}


__declspec( noinline ) static void
Profiler( int rank, unsigned long long ns ) noexcept
{
	static ptrdiff_t idx = -1;
//#ifndef __INTEL_OFFLOAD
# ifdef __EXCEPTIONS
	try
	{
# endif
		// send performance counters to the control process
		std::clog << "Process #" << rank << ": Sending Idx " << idx << " to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending dSquares " << dSquares / mega << "M to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending SquaresCnt " << SquaresCnt / mega << "M to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending Elapsed Time " << nsec( ns ) << " s to Master" << std::endl;

		MPI_Send( &idx, 1, MPI_OFFSET, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &dSquares, 1, MPI_LONG_LONG, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &SquaresCnt, 1, MPI_LONG_LONG, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &ns, 1, MPI_LONG_LONG, 0, 0, MPI_COMM_WORLD );
# ifdef __EXCEPTIONS
	}
	catch ( ... )
	{
	}
# endif
//#endif		// !__INTEL_OFFLOAD
	idx = idx - 1 | ~( ~static_cast< size_t >( 0 ) >> 1 );
}


static void Reset( );
static void Generate( int rank );

//#pragma offload_attribute( pop )	// offload_attribute( target( mic ) )


static size_t total_processed_wus = 0;
static size_t cur_launch_solved_wus = 0;

static int controlProcess( int procnum, const std::string &ifilename );
static bool sendWU( std::vector< wu > &wu_vec, ptrdiff_t &wu_start, int computing_process_id );
static int computingProcess( int rank );
static void writeLogFile( std::ostream &_os, const std::vector< wu > &wu_vec, ptrdiff_t wu_index, double &total_sqrs );
static void writeCurStateSstream( const std::vector< wu > &wu_vec, std::stringstream &sstream_search_space_state_cur );

};		// namespace dls9


using dls9::log_line;
using dls9::log_space;
using dls9::log_star;
using dls9::log_quote;

using dls9::s_name;
using dls9::s_keys;
using dls9::s_spaces;

using dls9::perf_limit;

using dls9::nsec;

using dls9::controlProcess;
using dls9::computingProcess;


int
main( int argc, char *argv[ ] )
{
	nanotimer( true );

	std::cerr.unsetf( std::ios_base::unitbuf );

	int namelen;
	char name[ MPI_MAX_PROCESSOR_NAME ];
	std::string ifilename, logfilename;
	std::fstream logfile;
	int rank = 0, procnum = 0, res = 0;

//	logfile.exceptions( );

//	#pragma offload_transfer target( mic )

	if ( MPI_Init( &argc, &argv ) != MPI_SUCCESS )
	{
		std::cerr << "Failed to initialize MPI" << std::endl;
		return EXIT_FAILURE;
	}

	// Create the communicator, and retrieve the number of processes.
	MPI_Comm_size( MPI_COMM_WORLD, &procnum );
	// Determine the rank of the process.
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	// Get machine name
	MPI_Get_processor_name( name, &namelen );

	while ( !res && ( ++argv, --argc ) )
	{
//	# ifndef __INTEL_OFFLOAD
		if ( std::strspn( *argv, s_keys ) )
		{
			if ( !isdigit( ( *argv )[ 1 ] ) )
				++res;
			else
			{
				char *pc = NULL;
				errno = 0;
				perf_limit = std::strtoul( *argv + 1, &pc, 10 );
				if ( errno || !perf_limit || !pc || *( pc + std::strspn( pc, s_spaces ) ) )
					++res;
			}
			continue;
		}
//	# endif		// !__INTEL_OFFLOAD
		if ( ifilename.empty( ) )
		{
			ifilename = *argv;
			continue;
		}
		if ( logfilename.empty( ) )
		{
			logfilename = *argv;
			logfile.exceptions( );
			logfile.open( *argv, std::ios_base::out | std::ios_base::app );
			if ( !logfile )
				++res;
			else
				std::clog.rdbuf( logfile.rdbuf( ) );
			continue;
		}
		++res;
	}
	if ( !res && ifilename.empty( ) )
		++res;

	uint64_t elapsed = nanotimer( false );

	if ( !rank )
	{
		std::cerr << "DLS9 - Optimized MPI version of the DLS enumeration application - " << __DATE__ << std::endl
			<< "Copyright (c) 2016 by Alexander M. Albertian, <assa@4ip.ru>." << std::endl
			<< "  This program comes with ABSOLUTELY NO WARRANTY." << std::endl
			<< "  This is free software, see the GNU General Public License for more details." << std::endl
			<< std::endl;

		if ( res )
			std::cerr << "Usage:" << std::endl
				<< "  mpiexec -np <nproc> [...] " << s_name << " <work-state> [<log-file>]"
//	#ifndef __INTEL_OFFLOAD
				<< " [-<nbench>]"
//	#endif
				<< std::endl;
		else
		{
			std::cerr << std::setfill( log_star ) << std::setw( log_line ) << " Processes: " << std::setfill( log_space ) <<
				procnum << std::endl;
			std::cerr << std::setfill( log_star ) << std::setw( log_line ) << " Working State: " << std::setfill( log_space ) <<
				log_quote << ifilename << log_quote << std::endl;
//	#ifndef __INTEL_OFFLOAD
			std::cerr << std::setfill( log_star ) << std::setw( log_line ) << " Benchmarking: " << std::setfill( log_space ) <<
				perf_limit << ( perf_limit ? " (ON)" : " (OFF)" ) << std::endl;
//	#endif
			if ( procnum < 2 )
				++res, std::cerr << "Number of processes must be 2 or more!" << std::endl;
			else
			{
				std::clog << "Master Process #" << rank << " on " << name << ", Init time: " << nsec( elapsed ) << " s" << std::endl;
				// MPI searching for DLS and constucting pseudotriples
				res = controlProcess( procnum, ifilename );
			}
		}
	}
	else
	{
		if ( res )
			res = 0;
		else
			if ( rank >= procnum )
				std::clog << "Idle Process #" << rank << " on " << name << ", Init time: " << nsec( elapsed ) << " s" << std::endl;
			else
			{
				std::clog << "Computing Process #" << rank << " on " << name << ", Init time: " << nsec( elapsed ) << " s" << std::endl;;
				res = computingProcess( rank );
			}
	}

	if ( res || perf_limit && !rank )
		MPI_Abort( MPI_COMM_WORLD, res );
	else
	{
		std::cerr << "Finalizing Process " << rank << std::endl;
		MPI_Finalize( );
	}
	return res;
}


namespace dls9
{

static int
controlProcess( int procnum, const std::string &ifilename )
{
//	std::string control_process_ofile_name = "control_process_ofile";
	std::string prev_state_file_name = ifilename + "prev";
	std::fstream prev_state_file, cur_state_file;
	std::vector< wu > wu_vec;	// cells filling and its result (calculated number)
	wu cur_wu;

	cur_state_file.exceptions( );
	cur_state_file.open( ifilename, std::ios_base::in );
	if ( !cur_state_file )
	{
		std::cerr << "Master: Error opening input file: " << ifilename << std::endl;
		return EXIT_FAILURE;
	}

	std::string str;
	std::stringstream sstream;

	// read current states of WUs from the input file
	wu_vec.reserve( 0x100000 );
	ptrdiff_t wu_index = 0, wu_start = -1, wu_state = 0;
	for ( sstream.exceptions( ); getline( cur_state_file, str ) && ++wu_index > 0; )
	{
		sstream << str;
		sstream >> cur_wu.first_cells_known_values >> cur_wu.dls_number;
		cur_wu.processing_time = -1.;
		sstream >> cur_wu.processing_time;	// get processing time if such exists
		sstream.str( std::string( ) ); sstream.clear( );
	//	cur_wu.start_time = ~static_cast< uint64_t >( 0 );
		if ( cur_wu.dls_number >= 0 )
		{
			++total_processed_wus;
			cur_wu.state = SOLVED;
		}
		else
		{
			cur_wu.state = NOT_STARTED;
			if ( wu_start < 0 )
				wu_start = wu_index - 1/*wu_vec.size( )*/;
		}
		wu_vec.push_back( cur_wu );
	}
	if ( !cur_state_file.eof( ) )
	{
		cur_state_file.close( );
		std::cerr << "Master: Error reading input file: " << ifilename << std::endl;
		return EXIT_FAILURE;
	}
	cur_state_file.close( ); cur_state_file.clear( );

	if ( !wu_vec.size( ) )
	{
		std::cerr << "Master: Invalid or empty input file: " << ifilename << std::endl;
		return EXIT_FAILURE;
	}
	if ( wu_index < 0 )
	{
		std::cerr << "Master: Seems like too many records in the file: " << ifilename << std::endl;
		return EXIT_FAILURE;
	}
	if ( wu_start < 0 )
	{
		std::cerr << "Master: No data to process in the file: " << ifilename << std::endl;
		return EXIT_SUCCESS;
	}

	/*uint64_t*/double sqrs = 0., total_sqrs = 0.;
	uint64_t elapsed, logged = nanotimer( true );

	writeLogFile( std::cout, wu_vec, -1, total_sqrs );

	MPI_Status status, current_status;
	//MPI_Request request;

	for ( int i = 1; i < procnum; ++i )
		sendWU( wu_vec, wu_start, i );

	std::stringstream sstream_search_space_state_prev, sstream_search_space_state_cur;

	sstream_search_space_state_prev.exceptions( );
	sstream_search_space_state_cur.exceptions( );
	writeCurStateSstream( wu_vec, sstream_search_space_state_cur );
	if ( sstream_search_space_state_cur.fail( ) )
	{
		std::cerr << "Master: stringstream I/O error" << std::endl;
		return EXIT_FAILURE;
	}

	// receive results and send back new WUs
	for ( int done = 1; total_processed_wus < wu_vec.size( ); )
	{
		// receive result
		wu_index = 0;
		MPI_Recv( &wu_index, 1, MPI_OFFSET, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status );
		current_status = status;

//	#ifdef __INTEL_OFFLOAD
//		if ( wu_index < 0 || wu_index >= wu_start/*wu_vec.size( )*/ || wu_vec[ wu_index ].state != IN_PROGRESS )
//	#else
		if ( wu_index >= wu_start/*wu_vec.size( )*/ || wu_index >= 0 && wu_vec[ wu_index ].state != IN_PROGRESS )
//	#endif
		{
			std::cerr << "Master: Invalid Idx received from #" << current_status.MPI_SOURCE << ": " << wu_index << std::endl;
			return EXIT_FAILURE;
		}

		MPI_Recv( &dSquares, 1, MPI_LONG_LONG, current_status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status );
		MPI_Recv( &SquaresCnt, 1, MPI_LONG_LONG, current_status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status );

//	#ifndef __INTEL_OFFLOAD
		if ( wu_index < 0 )
		{
			// momentary performance counter arrived
			uint64_t rank_tm = 0;
			MPI_Recv( &rank_tm, 1, MPI_LONG_LONG, current_status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status );

			elapsed = nanotimer( false );

			std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " Idx: " << wu_index << std::endl;
			std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " dSquares: " << dSquares / mega << 'M' << std::endl;
			std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " SquaresCnt: " << SquaresCnt / mega << 'M' << std::endl;
			std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " Elapsed Time: " << nsec( rank_tm ) << " s" << std::endl;
		//	std::clog << "Master: Process #" << current_status.MPI_SOURCE << " Elapsed Time: " << nsec( elapsed ) << " s" << std::endl;

			if ( SquaresCnt >= megax && elapsed - logged >= log_dly )
			{
				Logger( std::cout/*std::clog*/, current_status.MPI_SOURCE, '#', SquaresCnt, rank_tm );
				logged = elapsed;
			}

			sqrs += dSquares;

			if ( !wu_state )
				wu_state = wu_index;
			if ( ( done += wu_state != wu_index ) >= procnum )
			{
				Logger( std::cout/*std::clog*/, -1, '#', /*total_sqrs + */sqrs, elapsed );
				wu_state = 0;
				done = 1;
				if ( perf_limit == 1 )
				{
					std::cerr << "Master: Benchmark is complete" << std::endl;
					break;
				}
				--perf_limit;
			}

			continue;
		}
//	#endif		// !__INTEL_OFFLOAD

		MPI_Recv( FileData, sizeof( FileData ), MPI_CHAR, current_status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status );

		elapsed = nanotimer( false ) - wu_vec[ wu_index ].start_time;

		std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " Idx: " << wu_index << std::endl;
		std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " dSquares: " << dSquares / mega << 'M' << std::endl;
		std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " SquaresCnt: " << SquaresCnt / mega << 'M' << std::endl;
		std::clog << "Master: Received from #" << current_status.MPI_SOURCE << " \"" << FileData << "\"" << std::endl;
		std::clog << "Master: Process #" << current_status.MPI_SOURCE << " Elapsed Time: " << nsec( elapsed ) << " s" << std::endl;

		wu_vec[ wu_index ].processing_time = static_cast< double >( elapsed ) / static_cast< double >( nsec_multiplier );
		wu_vec[ wu_index ].dls_number = SquaresCnt;
	//	wu_vec[ wu_index ].end_procesing_time = MPI_Wtime( );
	//	wu_vec[ wu_index ].processing_time = wu_vec[ wu_index ].end_procesing_time - wu_vec[ wu_index ].start_processing_time;
		wu_vec[ wu_index ].state = SOLVED;

		// maintain consistency of the momentary squares counter
	//	sqrs -= SquaresCnt - dSquares;

		++total_processed_wus;
		++cur_launch_solved_wus;

		// send back a new WU (if exists)
		sendWU( wu_vec, wu_start, current_status.MPI_SOURCE );

		writeLogFile( std::cout, wu_vec, wu_index, total_sqrs );

		// update two working states - the current one and the reserved
	//	sstream_search_space_state_prev.str( std::string( ) ); sstream_search_space_state_prev.clear( );
		sstream_search_space_state_prev << sstream_search_space_state_cur.str( );
		sstream_search_space_state_cur.str( std::string( ) ); sstream_search_space_state_cur.clear( );
		writeCurStateSstream( wu_vec, sstream_search_space_state_cur );
		if ( sstream_search_space_state_prev.fail( ) || sstream_search_space_state_cur.fail( ) )
		{
			std::cerr << "Master: stringstream I/O error" << std::endl;
			return EXIT_FAILURE;
		}

		// don't change saved working state at benchmark
		if ( !perf_limit )
		{
			elapsed = nanotimer( false );
			cur_state_file.exceptions( );
			cur_state_file.open( ifilename, std::ios_base::out );
			if ( !cur_state_file ||
				( cur_state_file << sstream_search_space_state_cur.str( ), cur_state_file.fail( ) ) )
			{
				if ( cur_state_file )
					cur_state_file.close( );
				std::cerr << "Master: Error writing file: " << ifilename << std::endl;
				return EXIT_FAILURE;
			}
			cur_state_file.close( );// cur_state_file.clear( );
			elapsed = nanotimer( false ) - elapsed;

			std::clog << "= " << std::setw( log_line ) << "Working State recording time: " << nsec( elapsed ) << " s" << std::endl;

			prev_state_file.exceptions( );
			prev_state_file.open( prev_state_file_name, std::ios_base::out );
			if ( !prev_state_file ||
				( prev_state_file << sstream_search_space_state_prev.str( ), prev_state_file.fail( ) ) )
			{
				if ( prev_state_file )
					prev_state_file.close( );
				std::cerr << "Master: Error writing file: " << prev_state_file_name << std::endl;
				return EXIT_FAILURE;
			}
			prev_state_file.close( );// prev_state_file.clear( );
		}

		sstream_search_space_state_prev.str( std::string( ) ); sstream_search_space_state_prev.clear( );
	}

	std::cerr << "Master: Exiting Master Process" << std::endl;
	return EXIT_SUCCESS;
}

static void
writeLogFile( std::ostream &_os, const std::vector< wu > &wu_vec, ptrdiff_t wu_index, double &total_sqrs )
{
	uint64_t elapsed = nanotimer( false );
	double percent_val;
//	_os.clear( );
	_os.exceptions( );
	_os.unsetf( std::ios::floatfield | std::ios::adjustfield );
	_os.setf( std::ios::dec | std::ios::fixed );
	_os.precision( 6 );
	_os << std::endl << std::setfill( '=' ) << std::setw( log_line ) << "" << std::setfill( log_space ) << std::endl;
	_os << "= " << std::setw( log_line ) << "Total WUs: " << wu_vec.size( ) << std::endl;
	percent_val = static_cast< double >( total_processed_wus ) * 100. / static_cast<double>( wu_vec.size( ) );
	_os << "= " << std::setw( log_line ) << "total_processed_wus: " << total_processed_wus << " (" << percent_val << "%)" << std::endl;
	percent_val = static_cast< double >( cur_launch_solved_wus ) * 100. / static_cast<double>( wu_vec.size( ) );
	_os << "= " << std::setw( log_line ) << "cur_launch_solved_wus: " << cur_launch_solved_wus << " (" << percent_val << "%)" << std::endl;
	_os << "= " << std::endl;
	_os.flush( );
	_os.clear( );
	double hours = static_cast< double >( elapsed ) / ( static_cast< double >( nsec_multiplier ) * 3600. );
	_os.precision( 2 );
	_os << "= " << std::setw( log_line ) << "Elapsed Time: " << hours << " hrs" << std::endl;
	if ( cur_launch_solved_wus )
		_os << "= " << std::setw( log_line ) << "Estimated Time Remaining: " << hours * static_cast< double >( wu_vec.size( ) - total_processed_wus ) /
			static_cast< double >( cur_launch_solved_wus ) << " hrs" << std::endl;
	_os.precision( 6 );
	_os << "= " << std::endl;
	_os.flush( );
	_os.clear( );
	if ( wu_index != -1 && wu_vec[ wu_index ].dls_number )
	{
		total_sqrs += wu_vec[ wu_index ].dls_number;
	//	if ( wu_vec[ wu_index ].processing_time > 0. )
			Logger( _os, wu_index, '=', wu_vec[ wu_index ].dls_number, static_cast< uint64_t >( wu_vec[ wu_index ].processing_time * nsec_multiplier ) );
	//	if ( /*wu_index != -1 && */total_sqrs )
			Logger( _os, -1, '=', total_sqrs, elapsed );
	}
	_os.flush( );
//	_os.close( );
	_os.clear( );
}

static void
writeCurStateSstream( const std::vector< wu > &wu_vec, std::stringstream &sstream_search_space_state_cur )
{
	size_t k = 0;
	for ( const auto &x : wu_vec )
	{
		if ( k++ )
			sstream_search_space_state_cur << std::endl;
		sstream_search_space_state_cur << x.first_cells_known_values << log_space << x.dls_number << log_space;
		if ( x.processing_time >= .0 )
			sstream_search_space_state_cur << x.processing_time;
		else
			sstream_search_space_state_cur << "-1";
	}
}

static bool
sendWU( std::vector< wu > &wu_vec, ptrdiff_t &wu_start, int computing_process_id )
{
	ptrdiff_t first_non_started_wu_index = -1;
	std::vector< wu >::const_iterator next( wu_vec.cbegin( ) );
	std::vector< wu >::iterator wui( wu_vec.end( ) );

	// get index of the first unsolved WU
	if ( wu_start >= 0 && static_cast< size_t >( wu_start ) <= wu_vec.size( ) )
	{
		std::advance( next, wu_start );
		next = std::find_if( next, wu_vec.cend( ), [ ]( const wu &_wu ){ return _wu.state == NOT_STARTED; } );
		if ( next != wu_vec.cend( ) )
		{
			first_non_started_wu_index = std::distance( wu_vec.cbegin( ), next );
			wui = wu_vec.erase( next, next );
			// find next WU to process
			next = std::find_if( ++next, wu_vec.cend( ), [ ]( const wu &_wu ){ return _wu.state == NOT_STARTED; } );
		}
		wu_start = std::distance( wu_vec.cbegin( ), next );
	}

	std::clog << "Master: Sending Idx " << first_non_started_wu_index << " to #" << computing_process_id << std::endl;

	MPI_Send( &first_non_started_wu_index, 1, MPI_OFFSET, computing_process_id, 0, MPI_COMM_WORLD );

	if ( wui != wu_vec.end( ) )
	{
		std::copy( wui->first_cells_known_values.cbegin( ), wui->first_cells_known_values.cend( ), FileData );
		std::clog << "Master: Sending \"" << FileData << "\" to #" << computing_process_id << std::endl;

		MPI_Send( &FileData, sizeof( FileData ), MPI_BYTE, computing_process_id, 0, MPI_COMM_WORLD );

		wui->start_time/*_processing_time*/ = nanotimer( false );//MPI_Wtime( );
		wui->state = IN_PROGRESS;

		return true;
	}
	return false;
}

static int
computingProcess( int rank )
{
	MPI_Status status;
	ptrdiff_t wu_index;

	for ( dSquares = 0; ; )
	{
		// receive prefix from the control process
		wu_index = 0;
		MPI_Recv( &wu_index, 1, MPI_OFFSET, 0, 0, MPI_COMM_WORLD, &status );

		if ( status.MPI_SOURCE )
		{
			std::cerr << "Process #" << rank << ": Received from #" << status.MPI_SOURCE << ": Aborting!" << std::endl;
			return EXIT_FAILURE;
		}
		if ( wu_index == -1 )
		{
			std::cerr << "Process #" << rank << ": Received stop-message from Master" << std::endl;
			break;
		}
		if ( wu_index < 0 )
		{
			std::cerr << "Process #" << rank << ": Received from Master Idx is invalid: " << wu_index << ": Aborting!" << std::endl;
			return EXIT_FAILURE;
		}

		std::clog << "Process #" << rank << ": Received from Master Idx: " << wu_index << std::endl;

		MPI_Recv( &FileData, sizeof( FileData ), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status );

		std::clog << "Process #" << rank << ": Received from Master \"" << FileData << "\"" << std::endl;

		// calculate squares count in the current workunit
	//	#pragma offload target( mic ) \
	//		in( rank ) \
	//		inout( FileData, dSquares ) \
	//		out( SquaresCnt ) \
	//		nocopy( LS, CR, L, AD, MD, Rows, Columns )
		{
			SquaresCnt = /*dSquares = */0;
			Reset( );
			Generate( rank );
		}

		// send calculated result back to the control process
		std::clog << "Process #" << rank << ": Sending Idx " << wu_index << " to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending dSquares " << dSquares / mega << "M to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending SquaresCnt " << SquaresCnt / mega << "M to Master" << std::endl;
		std::clog << "Process #" << rank << ": Sending \"" << FileData << "\" to Master" << std::endl;

		MPI_Send( &wu_index, 1, MPI_OFFSET, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &dSquares, 1, MPI_LONG_LONG, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &SquaresCnt, 1, MPI_LONG_LONG, 0, 0, MPI_COMM_WORLD );
		MPI_Send( &FileData, sizeof( FileData ), MPI_BYTE, 0, 0, MPI_COMM_WORLD );
	}

	std::cerr << "Process #" << rank << ": Exiting" << std::endl;
	return EXIT_SUCCESS;
}


//#pragma offload_attribute( push, target( mic ) )

static void
Reset( )
{
	std::memset( &LS, 0, sizeof( LS ) );
	std::memset( &Columns, 0, sizeof( Columns ) );
	std::memset( &Rows, 0, sizeof( Rows ) );
	for ( uint32_t/*size_t*/ j = 0; j < N; ++j )
	{
		LS[ j ] = 1 << j;
		Rows[ 0 ] |= LS[ j ];
		Columns[ j ] |= LS[ j ];
	}
	MD = LS[ 0 ];
	AD = LS[ N - 1 ];
}

static void
Generate( int rank )
{
//	#pragma offload target( mic )
	{
		nanotimer( true );
		//for (CR[40] = AllN ^ (Rows[4] | Columns[4] | MD | AD); CR[40] != 0; CR[40] = (CR[40] & (CR[40] - 1))) {
		//LS[40] = (CR[40] & (-CR[40]));
		LS[40] = 1 << ((int)FileData[0] - 0x30); {
			Rows[4] |= LS[40];
			Columns[4] |= LS[40];
			MD |= LS[40];
			AD |= LS[40];
			//for (CR[10] = AllN ^ (Rows[1] | Columns[1] | MD); CR[10] != 0; CR[10] = (CR[10] & (CR[10] - 1))) {
			//LS[10] = (CR[10] & (-CR[10]));
			LS[10] = 1 << ((int)FileData[1] - 0x30); {
				Rows[1] |= LS[10];
				Columns[1] |= LS[10];
				MD |= LS[10];
				//for (CR[16] = AllN ^ (Rows[1] | Columns[7] | AD); CR[16] != 0; CR[16] = (CR[16] & (CR[16] - 1))) {
				//	LS[16] = (CR[16] & (-CR[16]));
				LS[16] = 1 << ((int)FileData[2] - 0x30); {
					Rows[1] |= LS[16];
					Columns[7] |= LS[16];
					AD |= LS[16];
					//for (CR[64] = AllN ^ (Rows[7] | Columns[1] | AD); CR[64] != 0; CR[64] = (CR[64] & (CR[64] - 1))) {
					//LS[64] = (CR[64] & (-CR[64]));
					LS[64] = 1 << ((int)FileData[3] - 0x30); {
						Rows[7] |= LS[64];
						Columns[1] |= LS[64];
						AD |= LS[64];
						//for (CR[70] = AllN ^ (Rows[7] | Columns[7] | MD); CR[70] != 0; CR[70] = (CR[70] & (CR[70] - 1))) {
						//LS[70] = (CR[70] & (-CR[70]));
						LS[70] = 1 << ((int)FileData[4] - 0x30); {
							Rows[7] |= LS[70];
							Columns[7] |= LS[70];
							MD |= LS[70];
							//for (CR[20] = AllN ^ (Rows[2] | Columns[2] | MD); CR[20] != 0; CR[20] = (CR[20] & (CR[20] - 1))) {
							//	LS[20] = (CR[20] & (-CR[20]));
							LS[20] = 1 << ((int)FileData[5] - 0x30); {
								Rows[2] |= LS[20];
								Columns[2] |= LS[20];
								MD |= LS[20];
								//for (CR[24] = AllN ^ (Rows[2] | Columns[6] | AD); CR[24] != 0; CR[24] = (CR[24] & (CR[24] - 1))) {
								//	LS[24] = (CR[24] & (-CR[24]));
								LS[24] = 1 << ((int)FileData[6] - 0x30); {
									Rows[2] |= LS[24];
									Columns[6] |= LS[24];
									AD |= LS[24];
									//for (CR[56] = AllN ^ (Rows[6] | Columns[2] | AD); CR[56] != 0; CR[56] = (CR[56] & (CR[56] - 1))) {
									//LS[56] = (CR[56] & (-CR[56]));
									LS[56] = 1 << ((int)FileData[7] - 0x30); {
										Rows[6] |= LS[56];
										Columns[2] |= LS[56];
										AD |= LS[56];
										//for (CR[60] = AllN ^ (Rows[6] | Columns[6] | MD); CR[60] != 0; CR[60] = (CR[60] & (CR[60] - 1))) {
										//	LS[60] = (CR[60] & (-CR[60]));
										LS[60] = 1 << ((int)FileData[8] - 0x30); {
											Rows[6] |= LS[60];
											Columns[6] |= LS[60];
											MD |= LS[60];
											//	for (CR[30] = AllN ^ (Rows[3] | Columns[3] | MD); CR[30] != 0; CR[30] = (CR[30] & (CR[30] - 1))) {
											//		LS[30] = (CR[30] & (-CR[30]));
											LS[30] = 1 << ((int)FileData[9] - 0x30); {
												Rows[3] |= LS[30];
												Columns[3] |= LS[30];
												MD |= LS[30];
												for (CR[32] = AllN ^ (Rows[3] | Columns[5] | AD); CR[32] != 0; CR[32] = (CR[32] & (CR[32] - 1))) {
													LS[32] = (CR[32] & (-CR[32]));
													Rows[3] |= LS[32];
													Columns[5] |= LS[32];
													AD |= LS[32];
													for (CR[48] = AllN ^ (Rows[5] | Columns[3] | AD); CR[48] != 0; CR[48] = (CR[48] & (CR[48] - 1))) {
														LS[48] = (CR[48] & (-CR[48]));
														Rows[5] |= LS[48];
														Columns[3] |= LS[48];
														AD |= LS[48];
														CR[72] = AllN ^ (Rows[8] | Columns[0] | AD);
														if (CR[72] != 0) {
															LS[72] = (CR[72] & (-CR[72]));
															Rows[8] |= LS[72];
															Columns[0] |= LS[72];
															for (CR[50] = AllN ^ (Rows[5] | Columns[5] | MD); CR[50] != 0; CR[50] = (CR[50] & (CR[50] - 1))) {
																LS[50] = (CR[50] & (-CR[50]));
																Rows[5] |= LS[50];
																Columns[5] |= LS[50];
																MD |= LS[50];
																CR[80] = AllN ^ (Rows[8] | Columns[8] | MD);
																if (CR[80] != 0) {
																	LS[80] = (CR[80] & (-CR[80]));
																	Rows[8] |= LS[80];
																	Columns[8] |= LS[80];
																	for (CR[11] = AllN ^ (Rows[1] | Columns[2]); CR[11] != 0; CR[11] = (CR[11] & (CR[11] - 1))) {
																		LS[11] = (CR[11] & (-CR[11]));
																		Rows[1] |= LS[11];
																		Columns[2] |= LS[11];
																		for (CR[12] = AllN ^ (Rows[1] | Columns[3]); CR[12] != 0; CR[12] = (CR[12] & (CR[12] - 1))) {
																			LS[12] = (CR[12] & (-CR[12]));
																			Rows[1] |= LS[12];
																			Columns[3] |= LS[12];
																			for (CR[14] = AllN ^ (Rows[1] | Columns[5]); CR[14] != 0; CR[14] = (CR[14] & (CR[14] - 1))) {
																				LS[14] = (CR[14] & (-CR[14]));
																				Rows[1] |= LS[14];
																				Columns[5] |= LS[14];
																				for (CR[15] = AllN ^ (Rows[1] | Columns[6]); CR[15] != 0; CR[15] = (CR[15] & (CR[15] - 1))) {
																					LS[15] = (CR[15] & (-CR[15]));
																					Rows[1] |= LS[15];
																					Columns[6] |= LS[15];
																					for (CR[9] = AllN ^ (Rows[1] | Columns[0]); CR[9] != 0; CR[9] = (CR[9] & (CR[9] - 1))) {
																						LS[9] = (CR[9] & (-CR[9]));
																						Rows[1] |= LS[9];
																						Columns[0] |= LS[9];
																						for (CR[13] = AllN ^ (Rows[1] | Columns[4]); CR[13] != 0; CR[13] = (CR[13] & (CR[13] - 1))) {
																							LS[13] = (CR[13] & (-CR[13]));
																							Rows[1] |= LS[13];
																							Columns[4] |= LS[13];
																							CR[17] = AllN ^ (Rows[1] | Columns[8]);
																							if (CR[17] != 0) {
																								LS[17] = (CR[17] & (-CR[17]));
																								Columns[8] |= LS[17];
																								for (CR[21] = AllN ^ (Rows[2] | Columns[3]); CR[21] != 0; CR[21] = (CR[21] & (CR[21] - 1))) {
																									LS[21] = (CR[21] & (-CR[21]));
																									Rows[2] |= LS[21];
																									Columns[3] |= LS[21];
																									for (CR[23] = AllN ^ (Rows[2] | Columns[5]); CR[23] != 0; CR[23] = (CR[23] & (CR[23] - 1))) {
																										LS[23] = (CR[23] & (-CR[23]));
																										Rows[2] |= LS[23];
																										Columns[5] |= LS[23];
																										for (CR[18] = AllN ^ (Rows[2] | Columns[0]); CR[18] != 0; CR[18] = (CR[18] & (CR[18] - 1))) {
																											LS[18] = (CR[18] & (-CR[18]));
																											Rows[2] |= LS[18];
																											Columns[0] |= LS[18];
																											for (CR[19] = AllN ^ (Rows[2] | Columns[1]); CR[19] != 0; CR[19] = (CR[19] & (CR[19] - 1))) {
																												LS[19] = (CR[19] & (-CR[19]));
																												Rows[2] |= LS[19];
																												Columns[1] |= LS[19];
																												for (CR[22] = AllN ^ (Rows[2] | Columns[4]); CR[22] != 0; CR[22] = (CR[22] & (CR[22] - 1))) {
																													LS[22] = (CR[22] & (-CR[22]));
																													Rows[2] |= LS[22];
																													Columns[4] |= LS[22];
																													for (CR[25] = AllN ^ (Rows[2] | Columns[7]); CR[25] != 0; CR[25] = (CR[25] & (CR[25] - 1))) {
																														LS[25] = (CR[25] & (-CR[25]));
																														Rows[2] |= LS[25];
																														Columns[7] |= LS[25];
																														CR[26] = AllN ^ (Rows[2] | Columns[8]);
																														if (CR[26] != 0) {
																															LS[26] = (CR[26] & (-CR[26]));
																															Columns[8] |= LS[26];
																															for (CR[57] = AllN ^ (Rows[6] | Columns[3]); CR[57] != 0; CR[57] = (CR[57] & (CR[57] - 1))) {
																																LS[57] = (CR[57] & (-CR[57]));
																																Rows[6] |= LS[57];
																																Columns[3] |= LS[57];
																																for (CR[59] = AllN ^ (Rows[6] | Columns[5]); CR[59] != 0; CR[59] = (CR[59] & (CR[59] - 1))) {
																																	LS[59] = (CR[59] & (-CR[59]));
																																	Rows[6] |= LS[59];
																																	Columns[5] |= LS[59];
																																	for (CR[54] = AllN ^ (Rows[6] | Columns[0]); CR[54] != 0; CR[54] = (CR[54] & (CR[54] - 1))) {
																																		LS[54] = (CR[54] & (-CR[54]));
																																		Rows[6] |= LS[54];
																																		Columns[0] |= LS[54];
																																		for (CR[55] = AllN ^ (Rows[6] | Columns[1]); CR[55] != 0; CR[55] = (CR[55] & (CR[55] - 1))) {
																																			LS[55] = (CR[55] & (-CR[55]));
																																			Rows[6] |= LS[55];
																																			Columns[1] |= LS[55];
																																			for (CR[58] = AllN ^ (Rows[6] | Columns[4]); CR[58] != 0; CR[58] = (CR[58] & (CR[58] - 1))) {
																																				LS[58] = (CR[58] & (-CR[58]));
																																				Rows[6] |= LS[58];
																																				Columns[4] |= LS[58];
																																				for (CR[61] = AllN ^ (Rows[6] | Columns[7]); CR[61] != 0; CR[61] = (CR[61] & (CR[61] - 1))) {
																																					LS[61] = (CR[61] & (-CR[61]));
																																					Rows[6] |= LS[61];
																																					Columns[7] |= LS[61];
																																					CR[62] = AllN ^ (Rows[6] | Columns[8]);
																																					if (CR[62] != 0) {
																																						LS[62] = (CR[62] & (-CR[62]));
																																						Columns[8] |= LS[62];
																																						for (CR[66] = AllN ^ (Rows[7] | Columns[3]); CR[66] != 0; CR[66] = (CR[66] & (CR[66] - 1))) {
																																							LS[66] = (CR[66] & (-CR[66]));
																																							Rows[7] |= LS[66];
																																							Columns[3] |= LS[66];
																																							for (CR[68] = AllN ^ (Rows[7] | Columns[5]); CR[68] != 0; CR[68] = (CR[68] & (CR[68] - 1))) {
																																								LS[68] = (CR[68] & (-CR[68]));
																																								Rows[7] |= LS[68];
																																								Columns[5] |= LS[68];
																																								for (CR[63] = AllN ^ (Rows[7] | Columns[0]); CR[63] != 0; CR[63] = (CR[63] & (CR[63] - 1))) {
																																									LS[63] = (CR[63] & (-CR[63]));
																																									Rows[7] |= LS[63];
																																									Columns[0] |= LS[63];
																																									for (CR[67] = AllN ^ (Rows[7] | Columns[4]); CR[67] != 0; CR[67] = (CR[67] & (CR[67] - 1))) {
																																										LS[67] = (CR[67] & (-CR[67]));
																																										Rows[7] |= LS[67];
																																										Columns[4] |= LS[67];
																																										for (CR[71] = AllN ^ (Rows[7] | Columns[8]); CR[71] != 0; CR[71] = (CR[71] & (CR[71] - 1))) {
																																											LS[71] = (CR[71] & (-CR[71]));
																																											Rows[7] |= LS[71];
																																											Columns[8] |= LS[71];
																																											for (CR[65] = AllN ^ (Rows[7] | Columns[2]); CR[65] != 0; CR[65] = (CR[65] & (CR[65] - 1))) {
																																												LS[65] = (CR[65] & (-CR[65]));
																																												Rows[7] |= LS[65];
																																												Columns[2] |= LS[65];
																																												CR[69] = AllN ^ (Rows[7] | Columns[6]);
																																												if (CR[69] != 0) {
																																													LS[69] = (CR[69] & (-CR[69]));
																																													Columns[6] |= LS[69];
																																													for (CR[75] = AllN ^ (Rows[8] | Columns[3]); CR[75] != 0; CR[75] = (CR[75] & (CR[75] - 1))) {
																																														LS[75] = (CR[75] & (-CR[75]));
																																														Rows[8] |= LS[75];
																																														Columns[3] |= LS[75];
																																														CR[39] = AllN ^ (Rows[4] | Columns[3]);
																																														if (CR[39] != 0) {
																																															LS[39] = (CR[39] & (-CR[39]));
																																															Rows[4] |= LS[39];
																																															for (CR[77] = AllN ^ (Rows[8] | Columns[5]); CR[77] != 0; CR[77] = (CR[77] & (CR[77] - 1))) {
																																																LS[77] = (CR[77] & (-CR[77]));
																																																Rows[8] |= LS[77];
																																																Columns[5] |= LS[77];
																																																CR[41] = AllN ^ (Rows[4] | Columns[5]);
																																																if (CR[41] != 0) {
																																																	LS[41] = (CR[41] & (-CR[41]));
																																																	Rows[4] |= LS[41];
																																																	for (CR[76] = AllN ^ (Rows[8] | Columns[4]); CR[76] != 0; CR[76] = (CR[76] & (CR[76] - 1))) {
																																																		LS[76] = (CR[76] & (-CR[76]));
																																																		Rows[8] |= LS[76];
																																																		Columns[4] |= LS[76];
																																																		for (CR[73] = AllN ^ (Rows[8] | Columns[1]); CR[73] != 0; CR[73] = (CR[73] & (CR[73] - 1))) {
																																																			LS[73] = (CR[73] & (-CR[73]));
																																																			Rows[8] |= LS[73];
																																																			Columns[1] |= LS[73];
																																																			for (CR[74] = AllN ^ (Rows[8] | Columns[2]); CR[74] != 0; CR[74] = (CR[74] & (CR[74] - 1))) {
																																																				LS[74] = (CR[74] & (-CR[74]));
																																																				Rows[8] |= LS[74];
																																																				Columns[2] |= LS[74];
																																																				for (CR[78] = AllN ^ (Rows[8] | Columns[6]); CR[78] != 0; CR[78] = (CR[78] & (CR[78] - 1))) {
																																																					LS[78] = (CR[78] & (-CR[78]));
																																																					Rows[8] |= LS[78];
																																																					Columns[6] |= LS[78];
																																																					CR[79] = AllN ^ (Rows[8] | Columns[7]);
																																																					if (CR[79] != 0) {
																																																						LS[79] = (CR[79] & (-CR[79]));
																																																						Columns[7] |= LS[79];
																																																						for (CR[31] = AllN ^ (Rows[3] | Columns[4]); CR[31] != 0; CR[31] = (CR[31] & (CR[31] - 1))) {
																																																							LS[31] = (CR[31] & (-CR[31]));
																																																							Rows[3] |= LS[31];
																																																							Columns[4] |= LS[31];
																																																							L[49] = Rows[5] | Columns[4];
																																																							if ((L[49] == AllN)) goto loop31end;
																																																							CR[49] = AllN ^ (Rows[5] | Columns[4]);
																																																							if (CR[49] != 0) {
																																																								LS[49] = (CR[49] & (-CR[49]));
																																																								Rows[5] |= LS[49];
																																																								for (CR[27] = AllN ^ (Rows[3] | Columns[0]); CR[27] != 0; CR[27] = (CR[27] & (CR[27] - 1))) {
																																																									LS[27] = (CR[27] & (-CR[27]));
																																																									Rows[3] |= LS[27];
																																																									Columns[0] |= LS[27];
																																																									L[28] = Rows[3] | Columns[1];
																																																									L[29] = Rows[3] | Columns[2];
																																																									L[33] = Rows[3] | Columns[6];
																																																									L[34] = Rows[3] | Columns[7];
																																																									L[35] = Rows[3] | Columns[8];
																																																									L[36] = Rows[4] | Columns[0];
																																																									L[45] = Rows[5] | Columns[0];
																																																									if ((L[28] == AllN) || (L[29] == AllN) || (L[33] == AllN) || (L[34] == AllN) || (L[35] == AllN) || (L[36] == AllN) || (L[45] == AllN)) goto loop27end;
																																																									for (CR[28] = AllN ^ (Rows[3] | Columns[1]); CR[28] != 0; CR[28] = (CR[28] & (CR[28] - 1))) {
																																																										LS[28] = (CR[28] & (-CR[28]));
																																																										Rows[3] |= LS[28];
																																																										Columns[1] |= LS[28];
																																																										L[29] = Rows[3] | Columns[2];
																																																										L[33] = Rows[3] | Columns[6];
																																																										L[34] = Rows[3] | Columns[7];
																																																										L[35] = Rows[3] | Columns[8];
																																																										L[37] = Rows[4] | Columns[1];
																																																										L[46] = Rows[5] | Columns[1];
																																																										if ((L[29] == AllN) || (L[33] == AllN) || (L[34] == AllN) || (L[35] == AllN) || (L[37] == AllN) || (L[46] == AllN)) goto loop28end;
																																																										for (CR[29] = AllN ^ (Rows[3] | Columns[2]); CR[29] != 0; CR[29] = (CR[29] & (CR[29] - 1))) {
																																																											LS[29] = (CR[29] & (-CR[29]));
																																																											Rows[3] |= LS[29];
																																																											Columns[2] |= LS[29];
																																																											L[33] = Rows[3] | Columns[6];
																																																											L[34] = Rows[3] | Columns[7];
																																																											L[35] = Rows[3] | Columns[8];
																																																											L[38] = Rows[4] | Columns[2];
																																																											L[47] = Rows[5] | Columns[2];
																																																											if ((L[33] == AllN) || (L[34] == AllN) || (L[35] == AllN) || (L[38] == AllN) || (L[47] == AllN)) goto loop29end;
																																																											for (CR[33] = AllN ^ (Rows[3] | Columns[6]); CR[33] != 0; CR[33] = (CR[33] & (CR[33] - 1))) {
																																																												LS[33] = (CR[33] & (-CR[33]));
																																																												Rows[3] |= LS[33];
																																																												Columns[6] |= LS[33];
																																																												L[34] = Rows[3] | Columns[7];
																																																												L[35] = Rows[3] | Columns[8];
																																																												L[42] = Rows[4] | Columns[6];
																																																												L[51] = Rows[5] | Columns[6];
																																																												if ((L[34] == AllN) || (L[35] == AllN) || (L[42] == AllN) || (L[51] == AllN)) goto loop33end;
																																																												for (CR[34] = AllN ^ (Rows[3] | Columns[7]); CR[34] != 0; CR[34] = (CR[34] & (CR[34] - 1))) {
																																																													LS[34] = (CR[34] & (-CR[34]));
																																																													Rows[3] |= LS[34];
																																																													Columns[7] |= LS[34];
																																																													L[35] = Rows[3] | Columns[8];
																																																													L[43] = Rows[4] | Columns[7];
																																																													L[52] = Rows[5] | Columns[7];
																																																													if ((L[35] == AllN) || (L[43] == AllN) || (L[52] == AllN)) goto loop34end;
																																																													CR[35] = AllN ^ (Rows[3] | Columns[8]);
																																																													if (CR[35] != 0) {
																																																														LS[35] = (CR[35] & (-CR[35]));
																																																														Columns[8] |= LS[35];
																																																														L[44] = Rows[4] | Columns[8];
																																																														L[53] = Rows[5] | Columns[8];
																																																														if ((L[44] == AllN) || (L[53] == AllN)) goto loop35end;
																																																														for (CR[36] = AllN ^ (Rows[4] | Columns[0]); CR[36] != 0; CR[36] = (CR[36] & (CR[36] - 1))) {
																																																															LS[36] = (CR[36] & (-CR[36]));
																																																															Rows[4] |= LS[36];
																																																															Columns[0] |= LS[36];
																																																															CR[45] = AllN ^ (Rows[5] | Columns[0]);
																																																															if (CR[45] != 0) {
																																																																LS[45] = (CR[45] & (-CR[45]));
																																																																Rows[5] |= LS[45];
																																																																for (CR[37] = AllN ^ (Rows[4] | Columns[1]); CR[37] != 0; CR[37] = (CR[37] & (CR[37] - 1))) {
																																																																	LS[37] = (CR[37] & (-CR[37]));
																																																																	Rows[4] |= LS[37];
																																																																	Columns[1] |= LS[37];
																																																																	CR[46] = AllN ^ (Rows[5] | Columns[1]);
																																																																	if (CR[46] != 0) {
																																																																		LS[46] = (CR[46] & (-CR[46]));
																																																																		Rows[5] |= LS[46];
																																																																		for (CR[38] = AllN ^ (Rows[4] | Columns[2]); CR[38] != 0; CR[38] = (CR[38] & (CR[38] - 1))) {
																																																																			LS[38] = (CR[38] & (-CR[38]));
																																																																			Rows[4] |= LS[38];
																																																																			Columns[2] |= LS[38];
																																																																			CR[47] = AllN ^ (Rows[5] | Columns[2]);
																																																																			if (CR[47] != 0) {
																																																																				LS[47] = (CR[47] & (-CR[47]));
																																																																				Rows[5] |= LS[47];
																																																																				for (CR[42] = AllN ^ (Rows[4] | Columns[6]); CR[42] != 0; CR[42] = (CR[42] & (CR[42] - 1))) {
																																																																					LS[42] = (CR[42] & (-CR[42]));
																																																																					Rows[4] |= LS[42];
																																																																					Columns[6] |= LS[42];
																																																																					CR[51] = AllN ^ (Rows[5] | Columns[6]);
																																																																					if (CR[51] != 0) {
																																																																						LS[51] = (CR[51] & (-CR[51]));
																																																																						Rows[5] |= LS[51];
																																																																						CR[43] = AllN ^ (Rows[4] | Columns[7]);
																																																																					//	#pragma vector always
																																																																					#ifdef EXPLICIT_LOOP_CONTER
																																																																					//	#pragma omp simd
																																																																					//	#pragma omp parallel for
																																																																						#pragma unroll (32)
																																																																						// Trying to explicitly compute the iteration count before executing the loop...
																																																																						for (unsigned i = _mm_popcnt_u32(CR[43]); i != 0; --i)
																																																																					#else
																																																																						while (CR[43] != 0)
																																																																					#endif
																																																																						{
																																																																							LS[43] = (CR[43] & (-CR[43]));
																																																																							Rows[4] |= LS[43];
																																																																							Columns[7] |= LS[43];
																																																																							CR[44] = AllN ^ (Rows[4] | Columns[8]);
																																																																							if (CR[44] != 0) {
																																																																								LS[44] = (CR[44] & (-CR[44]));
																																																																								Columns[8] |= LS[44];
																																																																								CR[52] = AllN ^ (Rows[5] | Columns[7]);
																																																																								if (CR[52] != 0) {
																																																																									LS[52] = (CR[52] & (-CR[52]));
																																																																									Rows[5] |= LS[52];
																																																																									CR[53] = AllN ^ (Rows[5] | Columns[8]);
																																																																									if (CR[53] != 0) {
																																																																									//	LS[53] = (CR[53] & (-CR[53]));
																																																																									//	Columns[8] |= LS[53];
																																																																										++SquaresCnt;
																																																																										if ( ++dSquares >= megax )
																																																																										{
																																																																									//		Logger( std::clog, rank, '#', SquaresCnt, nanotimer( false ) );
																																																																											Profiler( rank/*, dSquares, SquaresCnt*/, nanotimer( false ) );
																																																																											dSquares = 0;
																																																																										}
																																																																									//	Columns[8] ^= LS[53];
																																																																									}
																																																																									Rows[5] ^= LS[52];
																																																																								}
																																																																								Columns[8] ^= LS[44];
																																																																							}
																																																																							Rows[4] ^= LS[43];
																																																																							Columns[7] ^= LS[43];
																																																																							CR[43] = CR[43] & (CR[43] - 1);
																																																																						}
																																																																						Rows[5] ^= LS[51];
																																																																					}
																																																																					Rows[4] ^= LS[42];
																																																																					Columns[6] ^= LS[42];
																																																																				}
																																																																				Rows[5] ^= LS[47];
																																																																			}
																																																																			Rows[4] ^= LS[38];
																																																																			Columns[2] ^= LS[38];
																																																																		}
																																																																		Rows[5] ^= LS[46];
																																																																	}
																																																																	Rows[4] ^= LS[37];
																																																																	Columns[1] ^= LS[37];
																																																																}
																																																																Rows[5] ^= LS[45];
																																																															}
																																																															Rows[4] ^= LS[36];
																																																															Columns[0] ^= LS[36];
																																																														}
																																																													loop35end:
																																																														Columns[8] ^= LS[35];
																																																													}
																																																												loop34end:
																																																													Rows[3] ^= LS[34];
																																																													Columns[7] ^= LS[34];
																																																												}
																																																											loop33end:
																																																												Rows[3] ^= LS[33];
																																																												Columns[6] ^= LS[33];
																																																											}
																																																										loop29end:
																																																											Rows[3] ^= LS[29];
																																																											Columns[2] ^= LS[29];
																																																										}
																																																									loop28end:
																																																										Rows[3] ^= LS[28];
																																																										Columns[1] ^= LS[28];
																																																									}
																																																								loop27end:
																																																									Rows[3] ^= LS[27];
																																																									Columns[0] ^= LS[27];
																																																								}
																																																								Rows[5] ^= LS[49];
																																																							}
																																																						loop31end:
																																																							Rows[3] ^= LS[31];
																																																							Columns[4] ^= LS[31];
																																																						}
																																																						Columns[7] ^= LS[79];
																																																					}
																																																					Rows[8] ^= LS[78];
																																																					Columns[6] ^= LS[78];
																																																				}
																																																				Rows[8] ^= LS[74];
																																																				Columns[2] ^= LS[74];
																																																			}
																																																			Rows[8] ^= LS[73];
																																																			Columns[1] ^= LS[73];
																																																		}
																																																		Rows[8] ^= LS[76];
																																																		Columns[4] ^= LS[76];
																																																	}
																																																	Rows[4] ^= LS[41];
																																																}
																																																Rows[8] ^= LS[77];
																																																Columns[5] ^= LS[77];
																																															}
																																															Rows[4] ^= LS[39];
																																														}
																																														Rows[8] ^= LS[75];
																																														Columns[3] ^= LS[75];
																																													}
																																													Columns[6] ^= LS[69];
																																												}
																																												Rows[7] ^= LS[65];
																																												Columns[2] ^= LS[65];
																																											}
																																											Rows[7] ^= LS[71];
																																											Columns[8] ^= LS[71];
																																										}
																																										Rows[7] ^= LS[67];
																																										Columns[4] ^= LS[67];
																																									}
																																									Rows[7] ^= LS[63];
																																									Columns[0] ^= LS[63];
																																								}
																																								Rows[7] ^= LS[68];
																																								Columns[5] ^= LS[68];
																																							}
																																							Rows[7] ^= LS[66];
																																							Columns[3] ^= LS[66];
																																						}
																																						Columns[8] ^= LS[62];
																																					}
																																					Rows[6] ^= LS[61];
																																					Columns[7] ^= LS[61];
																																				}
																																				Rows[6] ^= LS[58];
																																				Columns[4] ^= LS[58];
																																			}
																																			Rows[6] ^= LS[55];
																																			Columns[1] ^= LS[55];
																																		}
																																		Rows[6] ^= LS[54];
																																		Columns[0] ^= LS[54];
																																	}
																																	Rows[6] ^= LS[59];
																																	Columns[5] ^= LS[59];
																																}
																																Rows[6] ^= LS[57];
																																Columns[3] ^= LS[57];
																															}
																															Columns[8] ^= LS[26];
																														}
																														Rows[2] ^= LS[25];
																														Columns[7] ^= LS[25];
																													}
																													Rows[2] ^= LS[22];
																													Columns[4] ^= LS[22];
																												}
																												Rows[2] ^= LS[19];
																												Columns[1] ^= LS[19];
																											}
																											Rows[2] ^= LS[18];
																											Columns[0] ^= LS[18];
																										}
																										Rows[2] ^= LS[23];
																										Columns[5] ^= LS[23];
																									}
																									Rows[2] ^= LS[21];
																									Columns[3] ^= LS[21];
																								}
																								Columns[8] ^= LS[17];
																							}
																							Rows[1] ^= LS[13];
																							Columns[4] ^= LS[13];
																						}
																						Rows[1] ^= LS[9];
																						Columns[0] ^= LS[9];
																					}
																					Rows[1] ^= LS[15];
																					Columns[6] ^= LS[15];
																				}
																				Rows[1] ^= LS[14];
																				Columns[5] ^= LS[14];
																			}
																			Rows[1] ^= LS[12];
																			Columns[3] ^= LS[12];
																		}
																		Rows[1] ^= LS[11];
																		Columns[2] ^= LS[11];
																	}
																	Rows[8] ^= LS[80];
																	Columns[8] ^= LS[80];
																}
																Rows[5] ^= LS[50];
																Columns[5] ^= LS[50];
																MD ^= LS[50];
															}
															Rows[8] ^= LS[72];
															Columns[0] ^= LS[72];
														}
														Rows[5] ^= LS[48];
														Columns[3] ^= LS[48];
														AD ^= LS[48];
													}
													Rows[3] ^= LS[32];
													Columns[5] ^= LS[32];
													AD ^= LS[32];
												}
												Rows[3] ^= LS[30];
												Columns[3] ^= LS[30];
												MD ^= LS[30];
											}
											Rows[6] ^= LS[60];
											Columns[6] ^= LS[60];
											MD ^= LS[60];
										}
										Rows[6] ^= LS[56];
										Columns[2] ^= LS[56];
										AD ^= LS[56];
									}
									Rows[2] ^= LS[24];
									Columns[6] ^= LS[24];
									AD ^= LS[24];
								}
								Rows[2] ^= LS[20];
								Columns[2] ^= LS[20];
								MD ^= LS[20];
							}
							Rows[7] ^= LS[70];
							Columns[7] ^= LS[70];
							MD ^= LS[70];
						}
						Rows[7] ^= LS[64];
						Columns[1] ^= LS[64];
						AD ^= LS[64];
					}
					Rows[1] ^= LS[16];
					Columns[7] ^= LS[16];
					AD ^= LS[16];
				}
				Rows[1] ^= LS[10];
				Columns[1] ^= LS[10];
				MD ^= LS[10];
			}
			Rows[4] ^= LS[40];
			Columns[4] ^= LS[40];
			MD ^= LS[40];
			AD ^= LS[40];
		}
	}
}

//#pragma offload_attribute( pop )	// offload_attribute( target( mic ) )

};		// namespace dls9
