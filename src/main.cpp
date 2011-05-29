#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include <algorithm>
#include <sstream>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <iterator>

#include "raw.h"

using namespace std;

const regex MatchTimeAny("[0-9]*[0-9]:[0-9][0-9]:*[0-9]*[0-9]*.*");
const regex MatchTimeExact("([0-9]*[0-9]):([0-9][0-9]):([0-9][0-9])");
const regex MatchTimeMinute("([0-9]*[0-9]):([0-9][0-9])");
const regex MatchTimeRange("([0-9]*[0-9]:[0-9][0-9]:[0-9][0-9])-([0-9]*[0-9]:[0-9][0-9]:[0-9][0-9])");

const string DefaultLogPath		= "logs\\haproxy.log";
const size_t MinTimestampLenght = 14;

streamoff	GetFileLength( ifstream& iff );
void		LineSeekBack( ifstream& iff );
uint32_t	TimestampToSeconds( const string& Timestamp );
streamoff	FindExactMatchAux( ifstream& iff, const uint32_t TargetSeconds, streamoff Lo, streamoff Hi );
void		FindExactMatches( ifstream& iff, const string& TargetTime );
void		FindRangeMatches( ifstream& iff, const string& TargetTimeBegin, const string& TargetTimeEnd );



void Usage()
{
	cout << "Usage:" << endl;
	cout << "Exact match:\t\ttgrep [PATH] HH:MM:SS" << endl;
	cout << "Match by minute:\ttgrep [PATH] HH:MM" << endl;
	cout << "Match in range:\t\ttgrep [PATH] HH:MM:SS-HH:MM:SS" << endl;
}


uint32_t TimestampToSeconds( const string& Timestamp )
{
	smatch TargetSubMatches;
	regex_search( Timestamp, TargetSubMatches, MatchTimeExact );
	return
		atoi(TargetSubMatches[1].str().c_str())*60*60 +
		atoi(TargetSubMatches[2].str().c_str())*60 +
		atoi(TargetSubMatches[3].str().c_str());
}

// Assuming "\r\n" newlines...
size_t LineSeekBegin( memmap_t* File, const char** Ptr )
{
	const char* FileBegin = (const char*)begin(File);
	const char* b = *Ptr;
	while ( FileBegin != b && '\n' != *b ) 
		--b;
	++b;	
	const size_t SeekLength = *Ptr - b;
	*Ptr = b;
	return SeekLength;
};

size_t LineSeekEnd( memmap_t* File, const char** Ptr )
{
	const char* FileEnd = (const char*)end(File);
	const char* e = *Ptr;
	while ( FileEnd != e && '\r' != *e ) 
		++e;
	const size_t SeekLength = e - *Ptr;
	*Ptr = e;
	return SeekLength;
};

const char* FindExactMatchAux( memmap_t* LogFile, const uint32_t TargetSeconds, const char* Lo, const char* Hi )
{
	if ( Lo > Hi )
	{
		cout << "ERROR: Time not found." << endl;
		return nullptr;
	}

	const char* Mid = Lo + (Hi - Lo) / 2;

	const char* b = Mid;
	const char* e = Mid;

	LineSeekBegin(LogFile, &b);
	LineSeekEnd(LogFile, &e);

	const uint32_t MatchSeconds = TimestampToSeconds( string( b, e ) );

	if ( TargetSeconds == MatchSeconds )
	{
		return b;
	}
	else if( TargetSeconds < MatchSeconds )
	{
		return FindExactMatchAux( LogFile, TargetSeconds, Lo, b );
	}
	else
	{
		return FindExactMatchAux( LogFile, TargetSeconds, e, Hi );
	}
}

void FindExactMatches( memmap_t* LogFile, const string& TargetTime )
{
	const auto Match = FindExactMatchAux( LogFile, TimestampToSeconds(TargetTime), 
		(const char*)begin(LogFile), (const char*)end(LogFile) );

	if ( Match )
	{
		vector<string> ReadLines;
		
		regex MatchTimestamp(string("... .. ")+TargetTime+string(".*"));

		const char* b = Match;
		const char* e = Match;

		LineSeekEnd(LogFile, &e);

		string LinePrev( b, e );
		do 
		{
			ReadLines.push_back(LinePrev);

			b -= 2;
			LineSeekBegin(LogFile, &b);
			e -= LinePrev.size() + 2; // skip "\r\n"

			LinePrev.assign( b, e );				
		} while ( regex_match( LinePrev, MatchTimestamp ) );

		for_each( ReadLines.rbegin(), ReadLines.rend(), [](const string& s) { cout << s << endl; } );

		b = Match + ReadLines.back().size() + 2;
		e = b;
		LineSeekEnd(LogFile, &e);

		string LineNext( b, e );
		while ( regex_match( LineNext, MatchTimestamp ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"
			e = b;
			LineSeekEnd(LogFile, &e);

			LineNext.assign( b, e );
		};
	}
}

void FindRangeMatches( memmap_t* LogFile, const string& TargetTimeBegin, const string& TargetTimeEnd )
{
	const auto MatchBegin = FindExactMatchAux( LogFile, TimestampToSeconds(TargetTimeBegin), 
		(const char*)begin(LogFile), (const char*)end(LogFile) );

	if ( !MatchBegin )
		return;

	const auto MatchEnd = FindExactMatchAux( LogFile, TimestampToSeconds(TargetTimeEnd), 
		MatchBegin, (const char*)end(LogFile) );

	if ( MatchEnd )
	{
		regex MatchTimestampBegin(string("... .. ")+TargetTimeBegin+string(".*"));
		regex MatchTimestampEnd(string("... .. ")+TargetTimeEnd+string(".*"));

		vector<string> ReadLines;

		const char* b = MatchBegin;
		const char* e = MatchBegin;

		LineSeekEnd(LogFile, &e);

		string LinePrev( b, e );
		do 
		{
			ReadLines.push_back(LinePrev);

			b -= 2;
			LineSeekBegin(LogFile, &b);
			e -= LinePrev.size() + 2; // skip "\r\n"

			LinePrev.assign( b, e );				
		} while ( regex_match( LinePrev, MatchTimestampBegin ) );

		for_each( ReadLines.rbegin(), ReadLines.rend(), [](const string& s) { cout << s << endl; } );

		b = MatchBegin + ReadLines.back().size() + 2;
		e = b;
		LineSeekEnd(LogFile, &e);

		string LineNext( b, e );
		while ( !regex_match( LineNext, MatchTimestampEnd ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"
			e = b;
			LineSeekEnd(LogFile, &e);

			LineNext.assign( b, e );
		};

		while ( regex_match( LineNext, MatchTimestampEnd ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"
			e = b;
			LineSeekEnd(LogFile, &e);

			LineNext.assign( b, e );
		};
	}
}

int GenTestLog()
{
	cout << "Generating test log file...";

	const uint32_t	Hours			= 60*60;
	const uint32_t	Minutes			= 60;
	const string	DatePrefix		= "Feb 10 ";
	const string	Postfixes[]		= { "egaewgawegkljh", "fo98fyf98", "F87Y3F7Y3F", "hdf8di7y", "387hf" };

	ofstream off(DefaultLogPath.c_str());

	if (!off.is_open())
	{
		cout << "ERROR: Could not open test log file." << endl;

		return 1;
	}

	ostringstream oss;
	oss << DatePrefix;	

	for ( uint32_t CurrTime = 0; CurrTime < (60*60*24); ++CurrTime )
	{			
		oss << CurrTime / Hours << ":";
		oss << setw(2) << setfill('0') << (CurrTime / Minutes)%60 << ":";
		oss << setw(2) << setfill('0') << CurrTime%60;

		const string Timestamp = oss.str();
		oss.seekp(DatePrefix.size());

		cout << CurrTime << endl;
		
		for( size_t ii = 10; ii != 0; --ii )
		{
			off << Timestamp << ' ' << Postfixes[rand()%5] << endl;
		}
	}

	cout << "Done!" << endl;

	return 0;
}

int main( int argc, char** argv )
{
	if ( 1 == argc )
	{
		Usage();
		return 1;
	}

	if ( 2 == argc && (string(argv[1]) == "-genlog") )
	{
		return GenTestLog();
	}
	
	string Path = DefaultLogPath;
	string TargetTime;
	
	if ( 2 == argc && regex_match( string(argv[1]), MatchTimeAny ) )
	{
		TargetTime = string(argv[1]);
	}
	else
	{
		string Arg1(argv[1]);
		string Arg2(argv[2]);

		if ( regex_match( Arg1, MatchTimeAny ) )
		{
			TargetTime = Arg1;
			Path = Arg2;
		}
		else if ( regex_match( Arg2, MatchTimeAny ) )
		{
			TargetTime = Arg2;
			Path = Arg1;
		}
		else
		{
			Usage();
			return 1;
		}
	}

	memmap_t* LogFile = open(Path.c_str());

	if ( !LogFile )
	{
		cout << "Failed to open logfile" << endl;
		return 1;
	}

	if ( regex_match( TargetTime, MatchTimeExact ) )
	{
		FindExactMatches( LogFile, TargetTime );
	}
	else if ( regex_match( TargetTime, MatchTimeRange ) )
	{
		smatch RangeSubMatches;
		if ( regex_search( TargetTime, RangeSubMatches, MatchTimeRange ) )
		{
			if ( TimestampToSeconds( RangeSubMatches[1].str()) < TimestampToSeconds( RangeSubMatches[2].str()) )
			{
				FindRangeMatches( LogFile, RangeSubMatches[1], RangeSubMatches[2] );
			}
			else
			{
				FindRangeMatches( LogFile, RangeSubMatches[2], RangeSubMatches[1] ); 
			}
		}		
	}	
	else if ( regex_match( TargetTime, MatchTimeMinute ) )
	{
		FindRangeMatches( LogFile, TargetTime + string(":00"), TargetTime + string(":59") );
	}

	close(LogFile);

	return 0;

	/*if ( regex_match( TargetTime, MatchTimeExact ) )
	{
	FindExactMatches( iff, TargetTime );
	}
	else if ( regex_match( TargetTime, MatchTimeRange ) )
	{
	smatch RangeSubMatches;
	if ( regex_search( TargetTime, RangeSubMatches, MatchTimeRange ) )
	{
	if ( TimestampToSeconds( RangeSubMatches[1].str()) < TimestampToSeconds( RangeSubMatches[2].str()) )
	{
	FindRangeMatches( iff, RangeSubMatches[1], RangeSubMatches[2] );
	}
	else
	{
	FindRangeMatches( iff, RangeSubMatches[2], RangeSubMatches[1] ); 
	}
	}		
	}	
	else if ( regex_match( TargetTime, MatchTimeMinute ) )
	{
	FindRangeMatches( iff, TargetTime + string(":00"), TargetTime + string(":59") );
	}*/
	
	return 0;
}
