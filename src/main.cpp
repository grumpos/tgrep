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

void LineSeekBack( ifstream& iff )
{
	streamoff Offset = iff.tellg();

	int c = iff.peek();
	while ( 0 != Offset && c != '\n' )
	{
		--Offset;
		iff.seekg(-1, ios_base::cur);
		c = iff.peek();
	}

	if ( 0 != Offset )
	{
		iff.ignore();
	}
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

streamoff FindExactMatchAux( ifstream& iff, const uint32_t TargetSeconds, streamoff Lo, streamoff Hi )
{
	if ( Lo > Hi )
	{
		cout << "ERROR: Time not found." << endl;
		return (streamoff)-1;
	}
	
	streamoff Mid = (Lo + Hi) / 2;
	iff.seekg(Mid);
	LineSeekBack(iff);
	Mid = iff.tellg();

	string Line;
	getline( iff, Line );

	const uint32_t MatchSeconds = TimestampToSeconds( Line );

	if ( TargetSeconds == MatchSeconds )
	{
		return Mid;
	}
	else if( TargetSeconds < MatchSeconds )
	{
		return FindExactMatchAux( iff, TargetSeconds, Lo, Mid-MinTimestampLenght );
	}
	else
	{
		return FindExactMatchAux( iff, TargetSeconds, iff.tellg(), Hi );
	}
}

streamoff GetFileLength( ifstream& iff )
{
	const streampos LastPos = iff.tellg();
	iff.seekg(0, ios_base::end);
	const streampos Length = iff.tellg();
	iff.seekg(LastPos);
	return Length;
}

void FindExactMatches( ifstream& iff, const string& TargetTime )
{
	const streamoff Match = FindExactMatchAux( iff, TimestampToSeconds(TargetTime), 0, GetFileLength(iff)-1 );

	if ( -1 != Match )
	{
		iff.seekg(Match);
		LineSeekBack( iff );

		streamoff Offset = iff.tellg();

		regex MatchTimestamp(string("... .. ")+TargetTime+string(".*"));

		while ( 0 != Offset )
		{
			string Line;
			getline(iff, Line);

			if ( !regex_match( Line, MatchTimestamp ) )
				break;

			iff.seekg(Offset-MinTimestampLenght);
			LineSeekBack( iff );
			Offset = iff.tellg();
		}

		string Line;
		while ( getline(iff, Line) && regex_match( Line, MatchTimestamp ) )
		{
			cout << Line << endl;
		}	
	}	
}

void FindRangeMatches( ifstream& iff, const string& TargetTimeBegin, const string& TargetTimeEnd )
{
	const streamoff MatchBegin	= FindExactMatchAux( iff, TimestampToSeconds(TargetTimeBegin), 0, GetFileLength(iff)-1 );
	
	if ( -1 == MatchBegin )
		return;

	const streamoff MatchEnd = FindExactMatchAux( iff, TimestampToSeconds(TargetTimeEnd), MatchBegin, GetFileLength(iff)-1 );

	if ( -1 != MatchEnd )
	{
		iff.seekg(MatchBegin);
		LineSeekBack( iff );

		streamoff Offset = iff.tellg();

		regex MatchTimestampBegin(string("... .. ")+TargetTimeBegin+string(".*"));
		regex MatchTimestampEnd(string("... .. ")+TargetTimeEnd+string(".*"));

		// Seek to first match for TargetTimeBegin
		while ( 0 != Offset )
		{
			string Line;
			getline(iff, Line);

			if ( !regex_match( Line, MatchTimestampBegin ) )
				break;

			iff.seekg(Offset-MinTimestampLenght);
			LineSeekBack( iff );
			Offset = iff.tellg();
		}

		// Print until first match for TargetTimeEnd
		string Line;
		while ( getline(iff, Line) && !regex_match( Line, MatchTimestampEnd ) )
		{
			cout << Line << endl;
		}	

		cout << Line << endl;

		// Print all match for TargetTimeEnd
		while ( getline(iff, Line) && regex_match( Line, MatchTimestampEnd ) )
		{
			cout << Line << endl;
		}
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

	ifstream iff( Path.c_str() );

	if ( !iff.is_open())
	{
		cout << "Error: Wrong path." << endl;
		return 1;
	}

	if ( 0 > GetFileLength( iff ) )
	{
		cout << "Error: File too big: " << GetFileLength( iff ) << "Max: " << INT32_MAX << endl;
		return 1;
	}

	if ( regex_match( TargetTime, MatchTimeExact ) )
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
	}
	
	return 0;
}
