
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sif.h"
#include "sifloader.h"
#include "sectSprites.h"
#include "sectStringArray.h"
#include "sif.fdh"

#ifdef NXENGINE_GW
#include "gw_malloc.h"
#endif

// safely make some vars sane while still staying POD
void SIFSprite::Init()
{
#ifdef NXENGINE_GW
	dirs = NULL;
#else
	frame = NULL;
#endif
}

// completely zero ALL vars
void SIFSprite::Zero()
{
	memset(this, 0, sizeof(SIFSprite));
}

void SIFSprite::FreeData()
{
#ifdef NXENGINE_GW
	/* dirs live in the RAM_EMU bump — no free. */
	dirs = NULL;
#else
	if (frame)
	{
		free(frame);
		frame = NULL;
	}
#endif
}

/*
void c------------------------------() {}
*/

void SIFSprite::CopyFrom(SIFSprite *other)
{
	*this = *other;
	
#ifdef NXENGINE_GW
	int n = nframes * ndirs;
	int copy_size = n * (int)sizeof(SIFDir);
	dirs = (SIFDir *)dtc_malloc(copy_size ? copy_size : sizeof(SIFDir));
	if (!dirs)
		dirs = (SIFDir *)ram_malloc(copy_size ? copy_size : sizeof(SIFDir));
	if (dirs && other->dirs)
		memcpy(dirs, other->dirs, copy_size);
#else
	int copy_size = (nframes * sizeof(SIFFrame));
	frame = (SIFFrame *)malloc(copy_size);
	memcpy(frame, other->frame, copy_size);
#endif
}


SIFSprite *SIFSprite::Duplicate()
{
	SIFSprite *spr = (SIFSprite *)malloc(sizeof(SIFSprite));
	spr->CopyFrom(this);
	
	return spr;
}

/*
void c------------------------------() {}
*/

void SIFSprite::AddFrame(SIFFrame *newframe)
{
#ifdef NXENGINE_GW
	(void)newframe;
	/* Editor path — unused on device. */
#else
	int frameno = nframes;
	SetNumFrames(nframes + 1);
	memcpy(&frame[frameno], newframe, sizeof(SIFFrame));
#endif
}

void SIFSprite::InsertFrame(SIFFrame *newframe, int insertbefore)
{
#ifdef NXENGINE_GW
	(void)newframe; (void)insertbefore;
#else
	if (insertbefore < 0) return;
	if (insertbefore >= nframes - 1)
	{
		AddFrame(newframe);
		return;
	}
	
	SIFFrame insertframe = *newframe;
	
	SetNumFrames(nframes + 1);
	
	int copy_len = ((nframes - 1) - insertbefore) * sizeof(SIFFrame);
	memmove(&frame[insertbefore+1], &frame[insertbefore], copy_len);
	
	frame[insertbefore] = insertframe;
#endif
}

void SIFSprite::DeleteFrame(int index)
{
#ifdef NXENGINE_GW
	(void)index;
#else
	if (index < 0 || index >= nframes)
		return;
	
	if (index < (nframes - 1))
	{
		int copy_len = ((nframes - 1) - index) * sizeof(SIFFrame);
		memmove(&frame[index], &frame[index+1], copy_len);
	}
	
	SetNumFrames(nframes - 1);
#endif
}

void SIFSprite::SetNumFrames(int newcount)
{
#ifdef NXENGINE_GW
	(void)newcount;
#else
	if (newcount == nframes) return;
	
	int required_size = (sizeof(SIFFrame) * newcount);
	if (frame) frame = (SIFFrame *)realloc(frame, required_size);
		  else frame = (SIFFrame *)malloc(required_size);
	
	if (newcount > nframes)
	{
		int blank_size = (newcount - nframes) * sizeof(SIFFrame);
		memset(&frame[nframes], 0, blank_size);
	}
	
	nframes = newcount;
	return;
#endif
}

SIFDir *SIFSprite::dir(int f, int d)
{
	if (f < 0 || f >= nframes) return NULL;
	if (d < 0 || d >= ndirs) return NULL;
#ifdef NXENGINE_GW
	if (dirs == NULL) return NULL;
	return &dirs[f * ndirs + d];
#else
	if (frame == NULL) return NULL;
	return &frame[f].dir[d];
#endif
}

/*
void c------------------------------() {}
*/
