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
#include "r_main.h"
#include "m_random.h"

#include "doomdef.h"

#include "SDL.h"


// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST		(1200*0x10000)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
#define S_CLOSE_DIST		(160*0x10000)


#define S_ATTENUATOR		((S_CLIPPING_DIST-S_CLOSE_DIST)>>FRACBITS)

// Adjustable by menu.
#define NORM_VOLUME    		snd_MaxVolume

#define NORM_PITCH     		128
#define NORM_PRIORITY		64
#define NORM_SEP		128

#define S_PITCH_PERTURB		1
#define S_STEREO_SWING		(96*0x10000)

// percent attenuation from front to back
#define S_IFRACVOL		30


// Needed for calling the actual sound output.
#define SAMPLECOUNT		1024
#define SAMPLERATE		44100	// Hz

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
	//Origin
	void *origin;
	
	//Sound effect info
	sfxinfo_t *sfx;
	
	//Playback state
	sfxbuffer_t *buffer;
	sfxfixed_t pos, inc;
	
	int left_vol, right_vol;
	
	//Linked list
	struct sfxchannel_s *prev, *next;
} sfxchannel_t;

sfxchannel_t *sfx_channel = NULL;

void I_ReleaseChannel(sfxchannel_t *channel)
{
	if (channel->prev != NULL)
		channel->prev->next = channel->next;
	if (channel->next != NULL)
		channel->next->prev = channel->prev;
	if (channel == sfx_channel)
		sfx_channel = channel->next;
	free(channel);
}

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
		
		//Get frames to mix before sound ends
		int frames_left = ((channel->buffer->size << 16) - channel->pos.value) / channel->inc.value;
		if (frames_left > frames)
			frames_left = frames;
		
		//Mix sound
		mixp = mix_stream;
		for (int i = 0; i < frames_left; i++)
		{
			Sint16 sample = (channel->buffer->data[channel->pos.upper] * (0x10000 - channel->pos.lower) + channel->buffer->data[channel->pos.upper + 1] * channel->pos.lower) >> 8;
			*mixp++ += (sample * channel->left_vol) >> 8;
			*mixp++ += (sample * channel->right_vol) >> 8;
			channel->pos.value += channel->inc.value;
		}
		
		if (channel->pos.upper >= channel->buffer->size)
			I_ReleaseChannel(channel);
		
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
	//size = W_LumpLength(sfxlump);
	
	//Get SFX data
	sfx = (unsigned char*)W_CacheLumpNum(sfxlump, PU_STATIC);
	
	//Read sound data
	buffer->size = *(Uint32*)(sfx + 0x4) - 32;
	buffer->freq = *(Uint16*)(sfx + 0x2);
	buffer->data = (char*)malloc(buffer->size + 1);
	
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
// Changes volume, stereo-separation, and pitch variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
int I_AdjustSoundParams
( mobj_t*	listener,
  mobj_t*	source,
  int*		vol,
  int*		sep,
  int*		pitch )
{
    fixed_t	approx_dist;
    fixed_t	adx;
    fixed_t	ady;
    angle_t	angle;

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = abs(listener->x - source->x);
    ady = abs(listener->y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);
    
    if (gamemap != 8
	&& approx_dist > S_CLIPPING_DIST)
    {
	return 0;
    }
    
    // angle of source to listener
    angle = R_PointToAngle2(listener->x,
			    listener->y,
			    source->x,
			    source->y);

    if (angle > listener->angle)
	angle = angle - listener->angle;
    else
	angle = angle + (0xffffffff - listener->angle);

    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    *sep = 128 - (FixedMul(S_STEREO_SWING,finesine[angle&FINEMASK])>>FRACBITS);

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
	*vol = snd_SfxVolume;
    }
    else if (gamemap == 8)
    {
	if (approx_dist > S_CLIPPING_DIST)
	    approx_dist = S_CLIPPING_DIST;

	*vol = 15+ ((snd_SfxVolume-15)
		    *((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
	    / S_ATTENUATOR;
    }
    else
    {
	// distance effect
	*vol = (snd_SfxVolume
		* ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
	    / S_ATTENUATOR; 
    }
    
    return (*vol > 0);
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
void I_ApplySoundParam(sfxchannel_t *channel, int vol, int sep)
{
	int tsep;
	
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
}

int
I_StartSound
( void*		origin_p,
  int		id,
  int		vol)
{
	int rc;
	int sep;
	int pitch;
	
	//Get volume, pitch, and panning
	mobj_t* origin = (mobj_t*)origin_p;
	sfxinfo_t *sfx = &S_sfx[id];
	
	//Initialize sound parameters
	if (sfx->link)
	{
		pitch = sfx->pitch;
		vol += sfx->volume;
		
		if (vol < 1)
			return 0;
		
		if (vol > snd_SfxVolume)
			vol = snd_SfxVolume;
	}
	else
	{
		pitch = NORM_PITCH;
	}
	
	// Check to see if it is audible,
	//  and if not, modify the params
	if (origin && origin != players[consoleplayer].mo)
	{
		rc = I_AdjustSoundParams(players[consoleplayer].mo, origin, &vol, &sep, &pitch);
		
		if (origin->x == players[consoleplayer].mo->x && origin->y == players[consoleplayer].mo->y)
			sep = NORM_SEP;
		
		if (!rc)
			return 0;
	}	
	else
	{
		sep = NORM_SEP;
	}
	
	//SFX random pitch
	if (id >= sfx_sawup && id <= sfx_sawhit)
	{	
		pitch += 8 - (M_Random() & 15);
		
		if (pitch < 0)
			pitch = 0;
		else if (pitch > 255)
			pitch = 255;
	}
	else if (id != sfx_itemup && id != sfx_tink)
	{
		pitch += 16 - (M_Random() & 31);
		
		if (pitch < 0)
			pitch = 0;
		else if (pitch > 255)
			pitch = 255;
	}
	
	//Create a new channel representing the given sound
	sfxchannel_t *channel = malloc(sizeof(sfxchannel_t));
	channel->origin = origin;
	channel->sfx = sfx;
	channel->buffer = &sfx_buffer[id];
	channel->pos.value = 0x00000000;
	channel->inc.value = 0x10000 * channel->buffer->freq / SAMPLERATE;//(Uint32)(pow(2.0, ((pitch-128.0)/64.0))*65536.0); //sound pitch randomization that doesn't happen in the DOS version, I assume becasue it sounds goofy as fuck
	
	I_ApplySoundParam(channel, vol, sep);
	
	//Lock audio device
	SDL_LockAudioDevice(SDL_audiodevice);
	
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

void I_UpdateSounds()
{
	int audible;
	int volume;
	int sep;
	int pitch;
	
	//Lock audio device
	SDL_LockAudioDevice(SDL_audiodevice);
	
	//Iterate through all sounds and update them
	for (sfxchannel_t *channel = sfx_channel; channel != NULL;)
	{
		sfxchannel_t *next = channel->next;
		
		//Initialize parameters
		sfxinfo_t *sfx = channel->sfx;
		
		volume = snd_SfxVolume;
		pitch = NORM_PITCH;
		sep = NORM_SEP;
		
		if (sfx->link)
		{
			pitch = sfx->pitch;
			volume += sfx->volume;
			
			if (volume < 1)
			{
				I_ReleaseChannel(channel);
				channel = next;
				continue;
			}
			else if (volume > snd_SfxVolume)
			{
				volume = snd_SfxVolume;
			}
		}
		
		// check non-local sounds for distance clipping
		//  or modify their params
		if (channel->origin && players[consoleplayer].mo != channel->origin)
		{
			audible = I_AdjustSoundParams(players[consoleplayer].mo,
						  channel->origin,
						  &volume,
						  &sep,
						  &pitch);
			
			if (!audible)
			{
				I_ReleaseChannel(channel);
				channel = next;
				continue;
			}
			else
			{
				I_ApplySoundParam(channel, volume, sep);
			}
		}
		
		channel = next;
	}
	
	//Unlock audio device
	SDL_UnlockAudioDevice(SDL_audiodevice);
}

void I_StopSound (void *origin)
{
	//Lock audio device
	SDL_LockAudioDevice(SDL_audiodevice);
	
	//Stop sounds under the given origin
	for (sfxchannel_t *channel = sfx_channel; channel != NULL;)
	{
		sfxchannel_t *next = channel->next;
		if (channel->origin == origin)
			I_ReleaseChannel(channel);
		channel = next;
	}
	
	//Unlock audio device
	SDL_UnlockAudioDevice(SDL_audiodevice);
}

void I_StopAllSound()
{
	//Release all channels
	for (sfxchannel_t *channel = sfx_channel; channel != NULL;)
	{
		sfxchannel_t *next = channel->next;
		free(channel);
		channel = next;
	}
	sfx_channel = NULL;
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
	
	//Release all channels
	I_StopAllSound();
	
	//Free sound buffer
	for (int i = 1; i < NUMSFX; i++) //SFX 0 is never initialized
		free(sfx_buffer[i].data);
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
