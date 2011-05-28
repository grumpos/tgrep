#pragma once
#ifndef __MEMMAP_H__
#define __MEMMAP_H__

struct memmap_t;

memmap_t*	open(const char* path);
void		close(memmap_t*);
size_t		size(memmap_t*);
void*		begin(memmap_t*);
void*		end(memmap_t*);

#endif