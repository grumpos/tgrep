#include <cassert>
#include <map>
#include <string>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "raw.h"

struct memmap_t
{
	void* begin_;
	void* end_;
};

static std::map<std::string, std::pair<size_t, memmap_t*>> MappedFiles;

memmap_t* open( const char* path )
{
	auto& MemmapRecord = MappedFiles[std::string(path)];

	if ( MemmapRecord.second )
	{
		++MemmapRecord.first;
	}
	else
	{
		HANDLE FileHandle = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
		HANDLE FileMapHandle = CreateFileMappingA( FileHandle, 0, PAGE_READONLY, 0, 0, 0 );
		LARGE_INTEGER FileSize = {0};
		GetFileSizeEx( FileHandle, &FileSize );
		memmap_t* rawmap_handle = new memmap_t;
		rawmap_handle->begin_	= MapViewOfFile( FileMapHandle, FILE_MAP_READ, 0, 0, 0 );
		rawmap_handle->end_		= (char*)rawmap_handle->begin_ + FileSize.QuadPart;
		CloseHandle( FileMapHandle );
		CloseHandle( FileHandle );

		MemmapRecord.first = 1;
		MemmapRecord.second = rawmap_handle;
	}

	return MemmapRecord.second;
}

void close(memmap_t* rawmap_handle)
{
	auto rawmap_handle_local = rawmap_handle;
	auto MemmapRecord = std::find_if( MappedFiles.begin(), MappedFiles.end(),
		[rawmap_handle_local]( const std::pair<std::string, std::pair<size_t, memmap_t*>>& rec ) -> bool
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