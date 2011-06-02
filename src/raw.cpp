#include <cassert>
#include <map>
#include <string>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "raw.h"

using namespace std;

struct memmap_t
{
	void* begin_;
	void* end_;
};

static map<string, pair<size_t, memmap_t*>> MappedFiles;

memmap_t* open( const char* path )
{
	auto& MemmapRecord = MappedFiles[string(path)];

	if ( MemmapRecord.second )
	{
		++MemmapRecord.first;
	}
	else
	{
		HANDLE FileHandle = CreateFileA( 
			path, 
			GENERIC_READ, 
			FILE_SHARE_READ, 
			0, 
			OPEN_EXISTING, 
			FILE_ATTRIBUTE_NORMAL, 
			0 );
		if ( FileHandle )
		{
			HANDLE FileMapHandle = CreateFileMappingA( 
				FileHandle, 
				0, 
				PAGE_READONLY, 
				0, 
				0, 
				0 );
			if ( FileMapHandle )
			{
				LARGE_INTEGER FileSize = {0};
				GetFileSizeEx( FileHandle, &FileSize );
				memmap_t* rawmap_handle = new memmap_t;
				rawmap_handle->begin_	= MapViewOfFile( FileMapHandle, FILE_MAP_READ, 0, 0, 0 );
				rawmap_handle->end_		= (char*)rawmap_handle->begin_ + FileSize.QuadPart;
				CloseHandle( FileMapHandle );

				MemmapRecord.first = 1;
				MemmapRecord.second = rawmap_handle;
			}
			CloseHandle( FileHandle );
		}		
	}

	return MemmapRecord.second;
}

void close(memmap_t* rawmap_handle)
{
	auto rawmap_handle_local = rawmap_handle;
	auto MemmapRecord = find_if( MappedFiles.begin(), MappedFiles.end(),
		[rawmap_handle_local]( const pair<string, pair<size_t, memmap_t*>>& rec ) -> bool
	{
		return rec.second.second == rawmap_handle_local;
	});

	if ( MemmapRecord != MappedFiles.end() )
	{
		if ( 1 == MemmapRecord->second.first )
		{
			memmap_t* m = MemmapRecord->second.second;
			UnmapViewOfFile( m->begin_ );
			delete m;
			MappedFiles.erase(MemmapRecord);
		}
		else
		{
			--MemmapRecord->second.first;
		}
	}
}

size_t	size(memmap_t* rm)
{ 
	return (char*)rm->end_ - (char*)rm->begin_; 
}

void*	begin(memmap_t* rm) 
{ 
	return  rm->begin_; 
}

void*	end(memmap_t* rm) 
{ 
	return  rm->end_; 
}
