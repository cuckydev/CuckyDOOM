// Emacs style mode select	 -*- C++ -*- 
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
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <SDL.h>

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <signal.h>

#include <errno.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#define POINTER_WARP_COUNTDOWN	1

SDL_Window *SDL_window = NULL;
SDL_Renderer *SDL_renderer = NULL;
SDL_Texture *SDL_gamesurf = NULL;
void (*SDL_surffunc)(void*, int) = NULL;

SDL_PixelFormat *SDL_palformat = NULL;
Uint32 SDL_gamepal[256];

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;


//
// Translate an SDL_KeyCode to DOOM keycodes
//
int TranslateKey(SDL_KeyCode keycode)
{
	//This code is rancid, but I have no idea how else I'm supposed to implement it
	switch(keycode)
	{
		case SDLK_LEFT:   return KEY_LEFTARROW;
		case SDLK_RIGHT:  return KEY_RIGHTARROW;
		case SDLK_DOWN:   return KEY_DOWNARROW;
		case SDLK_UP:     return KEY_UPARROW;
		case SDLK_ESCAPE: return KEY_ESCAPE;
		case SDLK_RETURN: return KEY_ENTER;
		case SDLK_TAB:    return KEY_TAB;
		case SDLK_F1:     return KEY_F1;
		case SDLK_F2:     return KEY_F2;
		case SDLK_F3:     return KEY_F3;
		case SDLK_F4:     return KEY_F4;
		case SDLK_F5:     return KEY_F5;
		case SDLK_F6:     return KEY_F6;
		case SDLK_F7:     return KEY_F7;
		case SDLK_F8:     return KEY_F8;
		case SDLK_F9:     return KEY_F9;
		case SDLK_F10:    return KEY_F10;
		case SDLK_F11:    return KEY_F11;
		case SDLK_F12:    return KEY_F12;
	
		case SDLK_BACKSPACE:
		case SDLK_DELETE:
			return KEY_BACKSPACE;
		
		case SDLK_PAUSE:
			return KEY_PAUSE;
		
		case SDLK_KP_EQUALS:
		case SDLK_KP_EQUALSAS400:
		case SDLK_EQUALS:
			return KEY_EQUALS;
		
		case SDLK_KP_MINUS:
		case SDLK_MINUS:
			return KEY_MINUS;
		
		case SDLK_LSHIFT:
		case SDLK_RSHIFT:
			return KEY_RSHIFT;
			break;
		
		case SDLK_LCTRL:
		case SDLK_RCTRL:
			return KEY_RCTRL;
		
		case SDLK_LALT:
		case SDLK_LGUI:
		case SDLK_RALT:
		case SDLK_RGUI:
			return KEY_RALT;
	}
	
	if (keycode < 0x80)
		return (int)keycode;
	
	printf("Unhandled SDL_KeyCode %d\n", keycode);
	return 0;
}

void I_ShutdownGraphics(void)
{
	//Free other rendering stuff
	if (SDL_palformat != NULL)
		SDL_FreeFormat(SDL_palformat);
	
	//Destroy game surface
	if (SDL_gamesurf != NULL)
		SDL_DestroyTexture(SDL_gamesurf);
	
	//Destroy renderer and window
	if (SDL_renderer != NULL)
		SDL_DestroyRenderer(SDL_renderer);
	if (SDL_window != NULL)
		SDL_DestroyWindow(SDL_window);
	
	//Quit SDL2 video backend
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
	
}

//static int	lastmousex = 0;
//static int	lastmousey = 0;
//boolean		mousemoved = false;
//boolean		shmFinished;

void I_GetEvent(void)
{
	event_t event;
	Uint32 x;
	
	SDL_Event SDL_event;
	while (SDL_PollEvent(&SDL_event))
	{
		switch (SDL_event.type)
		{
			case SDL_KEYDOWN:
				event.type = ev_keydown;
				event.data1 = TranslateKey(SDL_event.key.keysym.sym);
				D_PostEvent(&event);
				break;
			case SDL_KEYUP:
				event.type = ev_keyup;
				event.data1 = TranslateKey(SDL_event.key.keysym.sym);
				D_PostEvent(&event);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				event.type = ev_mouse;
				x = SDL_GetMouseState(NULL, NULL);
				event.data1 = 
				    ((x & SDL_BUTTON_LMASK) ? 1 : 0) |
				    ((x & SDL_BUTTON_RMASK) ? 2 : 0) |
				    ((x & SDL_BUTTON_MMASK) ? 4 : 0);
				event.data2 = 0;
				event.data3 = 0;
				D_PostEvent(&event);
				break;
			case SDL_MOUSEMOTION:
				event.type = ev_mouse;
				x = SDL_event.motion.state;
				event.data1 = 
				    ((x & SDL_BUTTON_LMASK) ? 1 : 0) |
				    ((x & SDL_BUTTON_RMASK) ? 2 : 0) |
				    ((x & SDL_BUTTON_MMASK) ? 4 : 0);
				event.data2 =  SDL_event.motion.xrel << 2;
				event.data3 = -SDL_event.motion.yrel << 2;
				D_PostEvent(&event);
				break;
		}
	}
}

//
// I_StartTic
//
void I_StartTic (void)
{
	
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
	// what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
	//Render DOOM screen to game surface
	void *buffer;
	int pitch;
	SDL_LockTexture(SDL_gamesurf, NULL, &buffer, &pitch);
	SDL_surffunc(buffer, pitch);
	SDL_UnlockTexture(SDL_gamesurf);
	
	//Render game surface to window
	SDL_RenderCopy(SDL_renderer, SDL_gamesurf, NULL, NULL);
	SDL_RenderPresent(SDL_renderer);
	
	//Handle events
	I_GetEvent();
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
	memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// Palette stuff.
//

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
	//Remap palette to RGB
	for (int i = 0; i < 256; i++)
	{
		byte r = *palette++; //Apparently SDL_MapRGB is a macro because doing this inline completely breaks it
		byte g = *palette++;
		byte b = *palette++;
		SDL_gamepal[i] = SDL_MapRGB(SDL_palformat, r, g, b);
	}
}

//
// Graphics initialization
//
#define TEMPLATE_SURFFUNC(SURFFUNC_NAME, SURFFUNC_TYPE) \
void SURFFUNC_NAME(void *buffer, int pitch) \
{ \
	SURFFUNC_TYPE *data = (SURFFUNC_TYPE*)buffer; \
	byte* screenp = screens[0]; \
	 \
	for (int i = 0; i < SCREENHEIGHT; i++) \
	{ \
		for (int x = 0; x < SCREENWIDTH; x++) \
			*data++ = SDL_gamepal[*screenp++]; \
		data += pitch - (SCREENWIDTH * sizeof(SURFFUNC_TYPE)); \
	} \
}

TEMPLATE_SURFFUNC(SDL_SurfFunc_1, Uint8)
TEMPLATE_SURFFUNC(SDL_SurfFunc_2, Uint16)
TEMPLATE_SURFFUNC(SDL_SurfFunc_4, Uint32)

void I_InitGraphics(void)
{
	//Attach SIGINT to quit event
	signal(SIGINT, (void (*)(int)) I_Quit);
	
	//Get game magnification
	if (M_CheckParm("-2"))
		multiply = 2;
	if (M_CheckParm("-3"))
		multiply = 3;
	if (M_CheckParm("-4"))
		multiply = 4;
	
	//Ensure SDL2 is initialized
	if (SDL_WasInit(SDL_INIT_EVERYTHING) != (SDL_INIT_EVERYTHING & ~SDL_INIT_NOPARACHUTE))
		SDL_Init(SDL_INIT_EVERYTHING);
	
	//Create window
	if ((SDL_window = SDL_CreateWindow("CuckyDOOM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREENWIDTH * multiply, SCREENHEIGHT * multiply, 0)) == NULL)
		I_Error((char*)SDL_GetError());
	
	//Create renderer
	if ((SDL_renderer = SDL_CreateRenderer(SDL_window, -1, 0)) == NULL)
		I_Error((char*)SDL_GetError());
	
	//Get palette format
	Uint32 window_format = SDL_GetWindowPixelFormat(SDL_window);
	if ((SDL_palformat = SDL_AllocFormat(window_format)) == NULL)
		I_Error((char*)SDL_GetError());
	
	switch (SDL_palformat->BytesPerPixel)
	{
		case 1:
			SDL_surffunc = SDL_SurfFunc_1;
			break;
		case 2:
			SDL_surffunc = SDL_SurfFunc_2;
			break;
		case 4:
			SDL_surffunc = SDL_SurfFunc_4;
			break;
		default:
			I_Error("Unsupported pixel depth");
			break;
	}
	
	//Create game surface
	if ((SDL_gamesurf = SDL_CreateTexture(SDL_renderer, window_format, SDL_TEXTUREACCESS_STREAMING, SCREENWIDTH, SCREENHEIGHT)) == NULL)
		I_Error((char*)SDL_GetError());
	
	//Lock mouse in window
	if (SDL_SetRelativeMouseMode(SDL_TRUE) < 0)
		I_Error((char*)SDL_GetError());
	
	//Allocate screen 0
	screens[0] = malloc(SCREENWIDTH * SCREENHEIGHT);
}
