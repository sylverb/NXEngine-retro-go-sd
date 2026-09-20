
#include <stdlib.h>
#include <string.h>

#include "StringList.h"
#include "StringList.fdh"

#ifdef NXENGINE_GW
#include "gw_mem.h"
#endif


StringList::~StringList()
{
	MakeEmpty();
}

/*
void c------------------------------() {}
*/

void StringList::Shuffle()
{
int i, count = CountItems();

	for(i=0;i<count;i++)
	{
		int swap = random(0, count - 1);
		if (swap != i)
		{
			SwapItems(i, swap);
		}
	}
}

bool StringList::ContainsString(const char *term)
{
int i;
char *str;

	for(i=0; (str = StringAt(i)); i++)
	{
		if (!strcmp(str, term))
			return true;
	}
	
	return false;
}

bool StringList::ContainsCaseString(const char *term)
{
int i;
char *str;

	for(i=0; (str = StringAt(i)); i++)
	{
		if (!strcasecmp(str, term))
			return true;
	}
	
	return false;
}

/*
void c------------------------------() {}
*/

void StringList::AddString(const char *str)
{
	BList::AddItem(strdup(str));
}

bool StringList::SetString(int index, const char *newstring)
{
	char *str = StringAt(index);
	if (!str) return 1;
	if (str == newstring) return 0;
	
	int copylen = strlen(newstring) + 1;
#ifdef NXENGINE_GW
	char *neu = (char *)gw_grow(str, strlen(str) + 1, (size_t)copylen);
	if (!neu)
		return 1;
	str = neu;
#else
	str = (char *)realloc(str, copylen);
	if (!str)
		return 1;
#endif
	memcpy(str, newstring, copylen);
	
	BList::ReplaceItem(index, str);
	return 0;
}

void StringList::RemoveString(int index)
{
	char *str = StringAt(index);
	if (str)
	{
		BList::RemoveItem(index);
#ifdef NXENGINE_GW
		gw_free_ahb(str);
#else
		free(str);
#endif
	}
}

void StringList::RemoveString(const char *str)
{
int i;
char *entry;

	for(i=0; (entry = StringAt(i)); i++)
	{
		if (!strcmp(entry, str))
		{
			BList::RemoveItem(i);
#ifdef NXENGINE_GW
			gw_free_ahb(entry);
#else
			free(entry);
#endif
			i--;
		}
	}
}

void StringList::RemoveIString(const char *str)
{
int i;
char *entry;

	for(i=0; (entry = StringAt(i)); i++)
	{
		if (!strcasecmp(entry, str))
		{
			BList::RemoveItem(i);
#ifdef NXENGINE_GW
			gw_free_ahb(entry);
#else
			free(entry);
#endif
			i--;
		}
	}
}

/*
void c------------------------------() {}
*/

void StringList::SwapItems(int index1, int index2)
{
	BList::SwapItems(index1, index2);
}

void StringList::DumpContents()
{
int i, count = CountItems();

	stat("StringList %08x; %d entries", this, count);
	for(i=0;i<count;i++)
	{
		char *str = StringAt(i);
		stat("(%d) <%08x>: '%s'", i, str, str ? str : "(null)");
	}
}

/*
void c------------------------------() {}
*/

char *StringList::StringAt(int index) const
{
	return (char *)BList::ItemAt(index);
}

void StringList::MakeEmpty()
{
	int i, count = CountItems();
	for(i=0;i<count;i++)
	{
#ifdef NXENGINE_GW
		gw_free_ahb(ItemAt(i));
#else
		free(ItemAt(i));
#endif
	}
	
	BList::MakeEmpty();
}

/*
void c------------------------------() {}
*/

StringList &StringList::operator= (const StringList &other)
{
	StringList::MakeEmpty();
	
	for(int i=0;;i++)
	{
		char *str = other.StringAt(i);
		if (!str) break;
		
		AddString(str);
	}
	
	return *this;
}

bool StringList::operator== (const StringList &other) const
{
	if (CountItems() != other.CountItems())
		return false;
	
	for(int i=0;;i++)
	{
		char *str1 = StringAt(i);
		char *str2 = other.StringAt(i);
		
		if (!str1 || !str2)
			return (!str1 && !str2);
		
		if (strcmp(str1, str2) != 0)
			return false;
	}
}

bool StringList::operator!= (const StringList &other) const
{
	return !(*this == other);
}


