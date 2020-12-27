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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <X11/XKBlib.h>

#include <X11/extensions/XShm.h>
// Had to dig up XShm.c for this one.
// It is in the libXext, but not in the XFree86 headers.
#ifdef LINUX
int XShmGetEventBase( Display* dpy ); // problems with g++?
#endif

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
		}
	}
	
	/*
	event_t event;

	// put event-grabbing stuff in here
	XNextEvent(X_display, &X_event);
	switch (X_event.type)
	{
		case KeyPress:
	event.type = ev_keydown;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "k");
	break;
		case KeyRelease:
	event.type = ev_keyup;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "ku");
	break;
		case ButtonPress:
	event.type = ev_mouse;
	event.data1 =
		(X_event.xbutton.state & Button1Mask)
		| (X_event.xbutton.state & Button2Mask ? 2 : 0)
		| (X_event.xbutton.state & Button3Mask ? 4 : 0)
		| (X_event.xbutton.button == Button1)
		| (X_event.xbutton.button == Button2 ? 2 : 0)
		| (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "b");
	break;
		case ButtonRelease:
	event.type = ev_mouse;
	event.data1 =
		(X_event.xbutton.state & Button1Mask)
		| (X_event.xbutton.state & Button2Mask ? 2 : 0)
		| (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
		event.data1
		^ (X_event.xbutton.button == Button1 ? 1 : 0)
		^ (X_event.xbutton.button == Button2 ? 2 : 0)
		^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "bu");
	break;
		case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
		(X_event.xmotion.state & Button1Mask)
		| (X_event.xmotion.state & Button2Mask ? 2 : 0)
		| (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
		lastmousex = X_event.xmotion.x;
		lastmousey = X_event.xmotion.y;
		if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
		{
		D_PostEvent(&event);
		// fprintf(stderr, "m");
		mousemoved = false;
		} else
		{
		mousemoved = true;
		}
	}
	break;
	
		case Expose:
		case ConfigureNotify:
	break;
	
		default:
	if (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
	}
	*/
}

//
// I_StartTic
//
void I_StartTic (void)
{
	if (!SDL_window)
		return;
	
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
	
/*
	char*		displayname;
	char*		d;
	int			n;
	int			pnum;
	int			x=0;
	int			y=0;
	
	// warning: char format, different type arg
	char		xsign=' ';
	char		ysign=' ';
	
	int			oktodraw;
	unsigned long	attribmask;
	XSetWindowAttributes attribs;
	XGCValues		xgcvalues;
	int			valuemask;
	static int		firsttime=1;

	if (!firsttime)
	return;
	firsttime = 0;

	signal(SIGINT, (void (*)(int)) I_Quit);

	if (M_CheckParm("-2"))
	multiply = 2;

	if (M_CheckParm("-3"))
	multiply = 3;

	if (M_CheckParm("-4"))
	multiply = 4;

	X_width = SCREENWIDTH * multiply;
	X_height = SCREENHEIGHT * multiply;

	// check for command-line display name
	if ( (pnum=M_CheckParm("-disp")) ) // suggest parentheses around assignment
	displayname = myargv[pnum+1];
	else
	displayname = 0;

	// check if the user wants to grab the mouse (quite unnice)
	grabMouse = !!M_CheckParm("-grabmouse");

	// check for command-line geometry
	if ( (pnum=M_CheckParm("-geom")) ) // suggest parentheses around assignment
	{
	// warning: char format, different type arg 3,5
	n = sscanf(myargv[pnum+1], "%c%d%c%d", &xsign, &x, &ysign, &y);
	
	if (n==2)
		x = y = 0;
	else if (n==6)
	{
		if (xsign == '-')
		x = -x;
		if (ysign == '-')
		y = -y;
	}
	else
		I_Error("bad -geom parameter");
	}

	// open the display
	X_display = XOpenDisplay(displayname);
	if (!X_display)
	{
	if (displayname)
		I_Error("Could not open display [%s]", displayname);
	else
		I_Error("Could not open display (DISPLAY=[%s])", getenv("DISPLAY"));
	}

	// use the default visual 
	X_screen = DefaultScreen(X_display);
	if (!XMatchVisualInfo(X_display, X_screen, 8, PseudoColor, &X_visualinfo))
	I_Error("xdoom currently only supports 256-color PseudoColor screens");
	X_visual = X_visualinfo.vi	//Get palette format
	Uint32 window_format = SDL_GetWindowPixelFormat(SDL_window);
	if ((SDL_palformat = SDL_AllocFormat(window_format)) == NULL)
		I_Error((char*)SDL_GetError());sual;

	// check for the MITSHM extension
	doShm = XShmQueryExtension(X_display);

	// even if it's available, make sure it's a local connection
	if (doShm)
	{
	if (!displayname) displayname = (char *) getenv("DISPLAY");
	if (displayname)
	{
		d = displayname;
		while (*d && (*d != ':')) d++;
		if (*d) *d = 0;
		if (!(!strcasecmp(displayname, "unix") || !*displayname)) doShm = false;
	}
	}

	fprintf(stderr, "Using MITSHM extension\n");

	// create the colormap
	X_cmap = XCreateColormap(X_display, RootWindow(X_display,
							 X_screen), X_visual, AllocAll);

	// setup attributes for main window
	attribmask = CWEventMask | CWColormap | CWBorderPixel;
	attribs.event_mask =
	KeyPressMask
	| KeyReleaseMask
	// | PointerMotionMask | ButtonPressMask | ButtonReleaseMask
	| ExposureMask;

	attribs.colormap = X_cmap;
	attribs.border_pixel = 0;

	// create the main window
	X_mainWindow = XCreateWindow(	X_display,
					RootWindow(X_display, X_screen),
					x, y,
					X_width, X_height,
					0, // borderwidth
					8, // depth
					InputOutput,
					X_visual,
					attribmask,
					&attribs );

	XDefineCursor(X_display, X_mainWindow,
			createnullcursor( X_display, X_mainWindow ) );

	// create the GC
	valuemask = GCGraphicsExposures;
	xgcvalues.graphics_exposures = False;
	X_gc = XCreateGC(	X_display,
				X_mainWindow,
				valuemask,
				&xgcvalues );

	// map the window
	XMapWindow(X_display, X_mainWindow);

	// wait until it is OK to draw
	oktodraw = 0;
	while (!oktodraw)
	{
	XNextEvent(X_display, &X_event);
	if (X_event.type == Expose
		&& !X_event.xexpose.count)
	{
		oktodraw = 1;
	}
	}

	// grabs the pointer so it is restricted to this window
	if (grabMouse)
	XGrabPointer(X_display, X_mainWindow, True,
			 ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
			 GrabModeAsync, GrabModeAsync,
			 X_mainWindow, None, CurrentTime);

	if (doShm)
	{

	X_shmeventtype = XShmGetEventBase(X_display) + ShmCompletion;

	// create the image
	image = XShmCreateImage(	X_display,
					X_visual,
					8,
					ZPixmap,
					0,
					&X_shminfo,
					X_width,
					X_height );

	grabsharedmemory(image->bytes_per_line * image->height);


	// UNUSED
	// create the shared memory segment
	// X_shminfo.shmid = shmget (IPC_PRIVATE,
	// image->bytes_per_line * image->height, IPC_CREAT | 0777);
	// if (X_shminfo.shmid < 0)
	// {
	// perror("");
	// I_Error("shmget() failed in InitGraphics()");
	// }
	// fprintf(stderr, "shared memory id=%d\n", X_shminfo.shmid);
	// attach to the shared memory segment
	// image->data = X_shminfo.shmaddr = shmat(X_shminfo.shmid, 0, 0);
	

	if (!image->data)
	{
		perror("");
		I_Error("shmat() failed in InitGraphics()");
	}

	// get the X server to attach to it
	if (!XShmAttach(X_display, &X_shminfo))
		I_Error("XShmAttach() failed in InitGraphics()");

	}
	else
	{
	image = XCreateImage(	X_display,
					X_visual,
					8,
					ZPixmap,
					0,
					(char*)malloc(X_width * X_height),
					X_width, X_height,
					8,
					X_width );

	}

	if (multiply == 1)
	screens[0] = (unsigned char *) (image->data);
	else
	screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);
*/
}
