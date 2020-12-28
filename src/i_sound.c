// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <math.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

#include "SDL.h"

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
#define SAMPLECOUNT		512
#define NUM_CHANNELS		8
#define SAMPLERATE		11025	// Hz

//Sound buffers
typedef struct
{
	char *data;
	unsigned short freq;
	unsigned long size;
} sfxbuffer_t;

sfxbuffer_t *sfx_buffer = NULL;

//Sound channels
typedef union
{
	struct
	{
		Uint16 lower, upper;
	};
	Uint32 value;
} sfxfixed_t;

typedef struct sfxchannel_s
{
	//Playback state
	sfxbuffer_t *buffer;
	sfxfixed_t pos, inc;
	
	int left_vol, right_vol;
	
	//Linked list
	struct sfxchannel_s *prev, *next;
} sfxchannel_t;

sfxchannel_t *sfx_channel = NULL;

//Audio device and callback
SDL_AudioDeviceID SDL_audiodevice;
Sint32 *mix_stream;

void I_AudioCallback(void* userdata, Uint8* buffer, int len)
{
	int samples = len / 2;
	int frames = len / 4;
	Sint32 *mixp;
	
	memset(mix_stream, 0, len * 2);
	
	//Mix sounds
	for (sfxchannel_t *channel = sfx_channel; channel != NULL;)
	{
		//Remember next channel in case channel is released
		sfxchannel_t *next = channel->next;
		
		//Mix sound
		mixp = mix_stream;
		for (int i = 0; i < frames; i++)
		{
			*mixp++ += channel->buffer->data[channel->pos.upper] * channel->left_vol;
			*mixp++ += channel->buffer->data[channel->pos.upper] * channel->right_vol;
			channel->pos.value += channel->inc.value;
			if (channel->pos.upper >= channel->buffer->size)
			{
				if (channel->prev != NULL)
					channel->prev->next = channel->next;
				if (channel->next != NULL)
					channel->next->prev = channel->prev;
				if (channel == sfx_channel)
					sfx_channel = channel->next;
				free(channel);
				break;
			}
		}
		
		//Mix next sound
		channel = next;
	}
	
	//Write mix stream to stream
	Uint16 *stream = (Uint16*)buffer;
	mixp = mix_stream;
	for (int i = 0; i < samples; i++)
	{
		if (*mixp < -0x7FFF)
			*stream++ = -0x7FFF;
		else if (*mixp > 0x7FFF)
			*stream++ = 0x7FFF;
		else
			*stream++ = *mixp;
		mixp++;
	}
}

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void I_LoadSoundData(sfxbuffer_t *buffer, char *sfxname)
{
	unsigned char *sfx;
	unsigned char *paddedsfx;
	int i;
	int size;
	int paddedsize;
	char name[20];
	int sfxlump;
	
	// Get the sound data from the WAD, allocate lump
	//  in zone memory.
	sprintf(name, "ds%s", sfxname);
	
	// Now, there is a severe problem with the
	//  sound handling, in it is not (yet/anymore)
	//  gamemode aware. That means, sounds from
	//  DOOM II will be requested even with DOOM
	//  shareware.
	// The sound list is wired into sounds.c,
	//  which sets the external variable.
	// I do not do runtime patches to that
	//  variable. Instead, we will use a
	//  default sound for replacement.
	if ( W_CheckNumForName(name) == -1 )
		sfxlump = W_GetNumForName("dspistol");
	else
		sfxlump = W_GetNumForName(name);
	
	//Get SFX data
	sfx = (unsigned char*)W_CacheLumpNum(sfxlump, PU_STATIC);
	
	//Read sound data
	buffer->size = *(Uint32*)(sfx + 0x4) - 32;
	buffer->freq = *(Uint16*)(sfx + 0x2);
	buffer->data = (char*)Z_Malloc(buffer->size + 1, PU_STATIC, 0);
	
	byte *datap = buffer->data;
	byte *sfxp = sfx + 0x18;
	for (int i = 0; i < buffer->size; i++)
		*datap++ = (*sfxp++ - 0x80);
	*datap++ = 0;
	
	// Remove the cached lump.
	Z_Free(sfx);
}

//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
  
}

 
void I_SetSfxVolume(int volume)
{
	snd_SfxVolume = volume;
}

// MUSIC API - dummy. Some code from DOS version.
void I_SetMusicVolume(int volume)
{
	snd_MusicVolume = volume;
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( void*		origin,
  int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
	int tsep;
	
	//Lock audio device
	SDL_LockAudioDevice(SDL_audiodevice);
	
	//Create a new channel representing the given sound
	sfxchannel_t *channel = malloc(sizeof(sfxchannel_t));
	channel->buffer = &sfx_buffer[id];
	channel->pos.value = 0x00000000;
	channel->inc.value = 0x10000 * channel->buffer->freq / SAMPLERATE;//(Uint32)(pow(2.0, ((pitch-128.0)/64.0))*65536.0); //sound pitch randomization that doesn't happen in the DOS version, I assume becasue it sounds goofy as fuck
	
	//Prepare volume
	vol *= 0x100 / 16;
	
	//Get left volume
	tsep = (sep - 128) * 2;
	if (tsep < 0)
		tsep = 0;
	if (tsep > 256)
		tsep = 256;
	channel->left_vol = vol * (256 - tsep) / 256;
	
	//Get left volume
	tsep = (128 - sep) * 2;
	if (tsep < 0)
		tsep = 0;
	if (tsep > 256)
		tsep = 256;
	channel->right_vol = vol * (256 - tsep) / 256;
	
	//Link channel
	channel->prev = NULL;
	channel->next = sfx_channel;
	if (sfx_channel != NULL)
		sfx_channel->prev = channel;
	sfx_channel = channel;
	
	//Unlock audio device
	SDL_UnlockAudioDevice(SDL_audiodevice);
	
	return 0;
}



void I_StopSound (int handle)
{
	
}


int I_SoundIsPlaying(int handle)
{
	return 0;
}

void I_ShutdownSound(void)
{
	//Close audio device
	SDL_CloseAudioDevice(SDL_audiodevice);
	
	//Free mix buffer
	free(mix_stream);
	
	//Free sound channels
	for (sfxchannel_t *channel = sfx_channel; channel != NULL;)
	{
		sfxchannel_t *next = channel->next;
		free(channel);
		channel = next;
	}
	
	//Free sound buffer
	for (int i = 1; i < NUMSFX; i++) //SFX 0 is never initialized
		Z_Free(sfx_buffer[i].data);
	free(sfx_buffer);
	
	//Quit SDL2 sound backend
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void I_InitSound()
{
	//Ensure SDL2 is initialized
	if (SDL_WasInit(SDL_INIT_EVERYTHING) != (SDL_INIT_EVERYTHING & ~SDL_INIT_NOPARACHUTE))
		SDL_Init(SDL_INIT_EVERYTHING);
	
	//Load sound buffers
	sfx_buffer = malloc(sizeof(sfxbuffer_t) * NUMSFX);
	for (int i = 1; i < NUMSFX; i++)
		I_LoadSoundData(&sfx_buffer[i], S_sfx[i].name);
	
	//Setup audio device
	SDL_AudioSpec want;
	SDL_zero(want);
	want.freq = SAMPLERATE;
	want.format = AUDIO_S16;
	want.channels = 2;
	want.samples = SAMPLECOUNT;
	want.callback = I_AudioCallback;
	
	if (!(SDL_audiodevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0)))
		I_Error((char*)SDL_GetError());
	
	//Allocate mix buffer
	mix_stream = malloc(4 * 2 * SAMPLECOUNT);
	
	//Start playing audio device
	SDL_PauseAudioDevice(SDL_audiodevice, 0);
}




//
// MUSIC API.
// Still no music done.
// Remains. Dummies.
//
void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }

static int	looping=0;
static int	musicdies=-1;

void I_PlaySong(int handle, int looping)
{
  // UNUSED.
  handle = looping = 0;
  musicdies = gametic + TICRATE*30;
}

void I_PauseSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_ResumeSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_StopSong(int handle)
{
  // UNUSED.
  handle = 0;
  
  looping = 0;
  musicdies = 0;
}

void I_UnRegisterSong(int handle)
{
  // UNUSED.
  handle = 0;
}

int I_RegisterSong(void* data)
{
  // UNUSED.
  data = NULL;
  
  return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  // UNUSED.
  handle = 0;
  return looping || musicdies > gametic;
}
