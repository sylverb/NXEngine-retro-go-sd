
#ifndef _VARARRAY_H
#define _VARARRAY_H

#include <stdlib.h>
#include <string.h>

#ifdef NXENGINE_GW
#include "common/gw_mem.h"
#endif

template <typename T>
class VarArray
{
public:
	VarArray()
	{
		nitems = 0;
		items = NULL;
	}
	
	~VarArray()
	{
		MakeEmpty();
	}
	
	// retrieves the item at index. if index is outside
	// the bounds of the array, returns 0/null.
	T get(int index)
	{
		if (index < 0 || index >= nitems)
			return (T)0;
		
		return items[index];
	}
	
	// put an item into the array at index.
	// if index is past the end of the array, the array is expanded.
	void put(int index, T value)
	{
		if (index < 0)
			return;
		EnsureAlloc(index + 1);
		/* OOM: EnsureAlloc left nitems unchanged */
		if (!items || index >= nitems)
			return;
		items[index] = value;
	}
	
	// make sure the array is big enough to contain up to allocnum items.
	// any unused items are initilized to 0/null.
	void EnsureAlloc(int allocnum)
	{
		if (allocnum <= nitems)
			return;

#ifdef NXENGINE_GW
		/* Bump pointer table — keep across TRA (MakeEmpty cannot reclaim bump).
		 * Double capacity to avoid O(n²) stranded copies on grow. */
		int newcap = nitems ? nitems : 16;
		while (newcap < allocnum) {
			if (newcap > 65536)
				return;
			newcap *= 2;
		}
		size_t nbytes = (size_t)newcap * sizeof(T);
		T *neu;
		if (!items)
			neu = (T *)gw_alloc(nbytes);
		else
			neu = (T *)gw_grow(items, (size_t)nitems * sizeof(T), nbytes);
		if (!neu)
			return;
		memset(&neu[nitems], 0, (size_t)(newcap - nitems) * sizeof(T));
		items = neu;
		nitems = newcap;
#else
		size_t nbytes = (size_t)allocnum * sizeof(T);
		T *neu;
		if (items == NULL)
			neu = (T *)malloc(nbytes);
		else
			neu = (T *)realloc(items, nbytes);
		if (!neu)
			return;
		memset(&neu[nitems], 0, (size_t)(allocnum - nitems) * sizeof(T));
		items = neu;
		nitems = allocnum;
#endif
	}
	
	// convenience function to access it like an array.
	// however unlike put(), you cannot use it to expand the array--
	// if you try to use it to write outside the bounds of the array,
	// the value will simply be lost.
	T& operator[] (const int index)
	{
		if (index < 0 || index >= nitems)
		{
			static T ZERO_T;
			memset(&ZERO_T, 0, sizeof(T));
			return ZERO_T;
		}
		
		return items[index];
	}
	
	// set the size of the array to 0 items.
	void MakeEmpty()
	{
		if (items)
		{
#ifdef NXENGINE_GW
			gw_free_ahb(items);
#else
			free(items);
#endif
			items = NULL;
		}
		
		nitems = 0;
	}
	
	int nitems;
	
private:
	T *items;
};




#endif
