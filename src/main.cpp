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
#include <vector>
#include "raw.h"

//#define __TAG printf("%d\n", __COUNTER__);

using namespace std;

const regex MatchTimeAny(	"\\d*\\d:\\d\\d:*\\d*\\d*.*");
const regex MatchTimeExact(	"(\\d*\\d):(\\d\\d):(\\d\\d)");
const regex MatchTimeMinute("(\\d*\\d):(\\d\\d)");
const regex MatchTimeRange(	"(\\d*\\d:\\d\\d:\\d\\d)-(\\d*\\d:\\d\\d:\\d\\d)");

const string DefaultLogPath		= "logs\\haproxy.log";
const size_t MinTimestampLenght = 14;
const string DefaultDelimiter	= "\r\n";

uint32_t	TimestampToSeconds( const string& Timestamp );

void Usage()
{
	cout << "Usage:" << endl;
	cout << "Exact match:\t\ttgrep [PATH] HH:MM:SS" << endl;
	cout << "Match by minute:\ttgrep [PATH] HH:MM" << endl;
	cout << "Match in range:\t\ttgrep [PATH] HH:MM:SS-HH:MM:SS" << endl;
}


uint32_t TimestampToSeconds( const string& Timestamp )
{
	smatch HHMMSS;

	if ( regex_search( Timestamp, HHMMSS, MatchTimeExact ) )
	{
		return
			atoi(HHMMSS[1].str().c_str())*60*60 +
			atoi(HHMMSS[2].str().c_str())*60 +
			atoi(HHMMSS[3].str().c_str());
	}

	return -1;
}

string SecondsToTimestamp( uint32_t Seconds )
{
	ostringstream oss;

	oss << Seconds / (60*60);

	oss << ":" << setw(2) << setfill('0') << (Seconds / 60) % 60 << ":";

	oss << ":" << setw(2) << setfill('0') << Seconds % 60;

	return oss.str();
}

// Assuming "\r\n" newlines...
size_t LineSeekBegin( memmap_t* File, const char** Ptr )
{
	const char* FileBegin = (const char*)begin(File);
	const char* b = *Ptr;

	while ( FileBegin != b && '\n' != *b )
	{
		--b;
	}

	if ( FileBegin != b )
	{
		++b;
	}
	
	*Ptr = b;

	const size_t BytesLeft = b - FileBegin;

	return BytesLeft;
};

size_t LineSeekEnd( memmap_t* File, const char** Ptr )
{
	const char* FileEnd = (const char*)end(File);
	const char* e = *Ptr;

	while ( FileEnd != e && '\r' != *e )
	{
		++e;
	}

	*Ptr = e;

	const size_t BytesLeft = FileEnd - e;

	return BytesLeft;
};

string GetLine( memmap_t* File, const char* Ptr, const char* Delimiter = "\r\n" )
{
	const char* const FileEnd = (const char*)end(File);
	const size_t DelimLength = strlen(Delimiter);
	const char* e = Ptr;

	while ( FileEnd > (e + DelimLength) && (0 != memcmp(Delimiter, e, DelimLength)) )
	{
		++e;
	}

	return string(Ptr, e);
}

const char* FindExactMatchHelper( memmap_t* LogFile, const uint32_t TargetSeconds, const char* Lo, const char* Hi )
{
	if ( Lo > Hi )
	{
		cout << "ERROR: Time not found: " << SecondsToTimestamp(TargetSeconds) << endl;

		return nullptr;
	}

	const char* Mid = Lo + ptrdiff_t(Hi - Lo) / 2;

	const char* b = Mid;
	const char* e = Mid;

	LineSeekBegin(LogFile, &b);
	LineSeekEnd(LogFile, &e);

	const uint32_t MatchSeconds = TimestampToSeconds( string(b, e) );

	if ( TargetSeconds == MatchSeconds )
	{
		return b;
	}
	else if( TargetSeconds < MatchSeconds )
	{
		return FindExactMatchHelper( LogFile, TargetSeconds, Lo, b );
	}
	else
	{
		return FindExactMatchHelper( LogFile, TargetSeconds, e, Hi );
	}
}

void PrintLinesBackward( memmap_t* LogFile, const char* StartOffset, const regex& Pattern )
{
	vector<string> ReadLines;
	
	const char* b = StartOffset;
	
	LineSeekBegin(LogFile, &b);

	string LinePrev;
	size_t BytesLeft = (size_t)-1;

	do
	{
		BytesLeft = LineSeekBegin(LogFile, &b);

		LinePrev = GetLine( LogFile, b );

		if ( !regex_match( LinePrev, Pattern ) )
			break;

		ReadLines.push_back(LinePrev);

		b -= 2; // skip "\r\n"
	} while ( BytesLeft );

	for_each( ReadLines.rbegin(), ReadLines.rend(), [](const string& s) { cout << s << endl; } );
}

void FindExactMatches( memmap_t* LogFile, const string& TargetTime )
{
	const auto Match = FindExactMatchHelper( LogFile, TimestampToSeconds(TargetTime), 
		(const char*)begin(LogFile), (const char*)end(LogFile) );

	if ( Match )
	{
		regex MatchTimestamp(string("... .. ")+TargetTime+string(".*"));

		PrintLinesBackward( LogFile, Match, MatchTimestamp );
		
		// Skip line of the match; already printed by PrintLinesBackward
		const char* NextLineBegin = Match;
		LineSeekEnd(LogFile, &NextLineBegin);
		NextLineBegin += DefaultDelimiter.length();

		const char* b = NextLineBegin;

		string LineNext = GetLine( LogFile, b );
		while ( regex_match( LineNext, MatchTimestamp ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"

			LineNext = GetLine( LogFile, b );
		};
	}
}

void FindRangeMatches( memmap_t* LogFile, const string& TargetTimeBegin, const string& TargetTimeEnd )
{
	const auto MatchBegin = FindExactMatchHelper( LogFile, TimestampToSeconds(TargetTimeBegin), 
		(const char*)begin(LogFile), (const char*)end(LogFile) );

	if ( !MatchBegin )
		return;

	const auto MatchEnd = FindExactMatchHelper( LogFile, TimestampToSeconds(TargetTimeEnd), 
		MatchBegin, (const char*)end(LogFile) );

	if ( MatchEnd )
	{
		regex MatchTimestampBegin(string("... .. ")+TargetTimeBegin+string(".*"));
		regex MatchTimestampEnd(string("... .. ")+TargetTimeEnd+string(".*"));

		PrintLinesBackward( LogFile, MatchBegin, MatchTimestampBegin );

		// Print between MatchBegin-MatchEnd
		const char* NextLineBegin = MatchBegin;
		LineSeekEnd(LogFile, &NextLineBegin);
		NextLineBegin += DefaultDelimiter.length();

		const char* b = NextLineBegin;

		string LineNext = GetLine( LogFile, b );
		while ( !regex_match( LineNext, MatchTimestampEnd ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"

			LineNext = GetLine( LogFile, b );
		};

		// Print past MatchEnd
		while ( regex_match( LineNext, MatchTimestampEnd ) )
		{
			cout << LineNext << endl;

			b += LineNext.size() + 2; // skip "\r\n"

			LineNext = GetLine( LogFile, b );
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

	for ( uint32_t Seconds = 0; Seconds < (60*60*24); ++Seconds )
	{			
		oss << SecondsToTimestamp( Seconds );

		const string Timestamp = oss.str();
		oss.seekp(DatePrefix.size());

		cout << Seconds << endl;
		
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
}
