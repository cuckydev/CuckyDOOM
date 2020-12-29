#include "SDL.h"

#include <string.h>
#include <unistd.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "d_main.h"

#include "cd_wadpick_bmp.inc.c"

// WAD Picker stuff
SDL_Window *wadpick_window;
SDL_Renderer *wadpick_renderer;
SDL_Texture *wadpick_texture;

int wadpick_select;

//Constants
#define WAD_NAME_X 60
#define WAD_TITLE_X 160
#define WAD_Y 48
#define WAD_Y_INC 16

//
// WAD files to look for
//
typedef struct
{
	const char *name;
	const char *title;
	GameMode_t gamemode;
	Language_t language;
} wadchk_t;

const wadchk_t wadchk[] = {
	{"doom.wad",     "DOOM Registered",         registered, english},
	{"doom1.wad",    "DOOM Shareware",          shareware,  english},
	{"doomu.wad",    "Ultimate DOOM",           retail,     english},
	{"doom2.wad",    "DOOM II",                 commercial, english},
	{"doom2f.wad",   "DOOM II (French)",        commercial, french},
	{"plutonia.wad", "The Plutonia Experiment", commercial, english},
	{"tnt.wad",      "TNT: Evilution",          commercial, english},
	{NULL, NULL, indetermined, english},
};

//
// WAD Picker drawing functions
//
void CD_WadPick_DrawTex(const SDL_Rect *src, int x, int y)
{
	SDL_Rect dst = {x, y, src->w, src->h};
	SDL_RenderCopy(wadpick_renderer, wadpick_texture, src, &dst);
}

static const unsigned char font_space[32*3] = {
	4,2,4,8,6,8,7,2,4,4,8,6,3,6,2,4,6,3,6,6,7,6,6,6,6,6,2,3,4,6,4,5,
	8,6,6,6,6,5,5,6,6,2,5,6,5,6,6,6,6,6,6,5,6,6,6,6,6,6,5,4,4,4,4,6,
	3,6,6,6,6,6,5,6,6,2,4,5,3,8,6,6,6,6,5,5,5,6,6,6,6,6,6,5,2,5,6,4,
};

int CD_WadPick_GetTextWidth(const char *text)
{
	int x = 0;
	unsigned char v;
	while ((v = (unsigned char)*text++ - 0x20) != 0xE0)
		if (v <= 0x60)
			x += font_space[v];
	return x - ((x != 0) ? 1 : 0);
}

void CD_WadPick_DrawText(int x, int y, const char *text, unsigned char r, unsigned char g, unsigned char b)
{
	//Set colour
	SDL_SetTextureColorMod(wadpick_texture, r, g, b);
	
	//Render characters
	SDL_Rect rect = {0, 0, 8, 12};
	unsigned char v;
	
	while ((v = (unsigned char)*text++ - 0x20) != 0xE0)
	{
		if (v == 0)
		{
			//Don't render spaces, waste of time
			x += font_space[v];
		}
		else if (v <= 0x60)
		{
			//Render character
			rect.x = (v & 0x1F) << 3;
			rect.y = 64 + ((v >> 5) * 12);
			CD_WadPick_DrawTex(&rect, x, y);
			x += font_space[v];
		}
	}
	
	//Restore colour
	SDL_SetTextureColorMod(wadpick_texture, 0xFF, 0xFF, 0xFF);
}

//
// WAD Picker
//

void CD_WadPick_UseWad(const wadchk_t *wad)
{
	char *path = malloc(strlen(executable_dir) + 1 + strlen(wad->name) + 1);
	sprintf(path, "%s/%s", executable_dir, wad->name);
	gamemode = wad->gamemode;
	language = wad->language;
	D_AddFile(path);
	free(path);
}

void CD_WadPicker()
{
	//Find available WADs
	const wadchk_t *wadpick_opt[sizeof(wadchk) / sizeof(wadchk_t)];
	int wadpick_opts = 0;
	
	for (const wadchk_t *wad = wadchk; wad->name != NULL; wad++)
	{
		//Get WAD path
		char *path = malloc(strlen(executable_dir) + 1 + strlen(wad->name) + 1);
		sprintf(path, "%s/%s", executable_dir, wad->name);
		
		//Check if WAD exists
		if (!access(path, R_OK))
		{
			wadpick_opt[wadpick_opts] = wad;
			wadpick_opts++;
		}
		free(path);
	}
	
	if (wadpick_opts == 0)
	{
		//Failed to find any WADs, no point in picking
		return;
	}
	
	//Initialize WAD picker state
	wadpick_select = 0;
	
	//Ensure SDL2 is initialized
	if (!SDL_WasInit(SDL_INIT_EVERYTHING))
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
			I_Error((char*)SDL_GetError());
	
	//Create WAD picker window and renderer
	wadpick_window = SDL_CreateWindow("CuckyDOOM - WAD Picker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREENWIDTH*2, SCREENHEIGHT*2, 0);
	if (wadpick_window == NULL)
		I_Error((char*)SDL_GetError());
	
	wadpick_renderer = SDL_CreateRenderer(wadpick_window, -1, 0);
	if (wadpick_renderer == NULL)
		I_Error((char*)SDL_GetError());
	
	SDL_RenderSetLogicalSize(wadpick_renderer, SCREENWIDTH, SCREENHEIGHT);
	
	//Load WAD pick sheet
	SDL_RWops *rw = SDL_RWFromConstMem(cd_wadpick_bmp, sizeof(cd_wadpick_bmp));
	if (rw == NULL)
		I_Error((char*)SDL_GetError());
	SDL_Surface *bmp_surface = SDL_LoadBMP_RW(rw, 1);
	if (bmp_surface == NULL)
		I_Error((char*)SDL_GetError());
	SDL_SetColorKey(bmp_surface, SDL_TRUE, 0);
	
	wadpick_texture = SDL_CreateTextureFromSurface(wadpick_renderer, bmp_surface);
	if (wadpick_texture == NULL)
		I_Error((char*)SDL_GetError());
	
	SDL_FreeSurface(bmp_surface);
	
	//Test
	boolean quit = false, exit = false;
	while (!(quit || exit))
	{
		//Handle events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					exit = true;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.scancode)
					{
						case SDL_SCANCODE_UP:
							if (--wadpick_select < 0)
								wadpick_select = 0;
							break;
						case SDL_SCANCODE_DOWN:
							if (++wadpick_select >= wadpick_opts)
								wadpick_select--;
							break;
						case SDL_SCANCODE_RETURN:
							CD_WadPick_UseWad(wadpick_opt[wadpick_select]);
							quit = true;
							break;
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						//Get the WAD option we just clicked
						int select = (event.button.y - WAD_Y) / WAD_Y_INC;
						if (select < 0 || select >= wadpick_opts)
							break;
						
						if (select == wadpick_select && event.button.clicks >= 2)
						{
							//If we're double-clicking the already selected WAD, use it
							CD_WadPick_UseWad(wadpick_opt[wadpick_select]);
							quit = true;
						}
						else
						{
							//Select clicked WAD
							wadpick_select = select;
						}
					}
					break;
				default:
					break;
			}
		}
		
		//Draw background
		SDL_Rect back = {64, 0, 64, 64};
		for (int y = 0; y < SCREENHEIGHT; y += back.h)
			for (int x = 0; x < SCREENWIDTH; x += back.w)
				CD_WadPick_DrawTex(&back, x, y);
		
		//Draw available WADs
		for (int i = 0; i < wadpick_opts; i++)
		{
			unsigned char shade = (i == wadpick_select) ? 0xFF : 0x80;
			CD_WadPick_DrawText(WAD_NAME_X,  WAD_Y + i * WAD_Y_INC + 1, wadpick_opt[i]->name,  0x10, 0x10, 0x10);
			CD_WadPick_DrawText(WAD_NAME_X,  WAD_Y + i * WAD_Y_INC,     wadpick_opt[i]->name,  shade, shade, shade);
			CD_WadPick_DrawText(WAD_TITLE_X, WAD_Y + i * WAD_Y_INC + 1, wadpick_opt[i]->title, 0x10, 0x10, 0x10);
			CD_WadPick_DrawText(WAD_TITLE_X, WAD_Y + i * WAD_Y_INC,     wadpick_opt[i]->title, shade, shade, shade);
		}
		
		//Draw foreground framing
		SDL_Rect frame = {0, 0, 64, 64};
		for (int x = 0; x < SCREENWIDTH; x += frame.w)
		{
			CD_WadPick_DrawTex(&frame, x, -32);
			CD_WadPick_DrawTex(&frame, x, SCREENHEIGHT - 32);
		}
		
		//Draw title bar
		static const char *title = "CuckyDOOM - Wad Picker";
		CD_WadPick_DrawText((SCREENWIDTH - CD_WadPick_GetTextWidth(title)) / 2, 11, title, 0x10, 0x10, 0x10);
		CD_WadPick_DrawText((SCREENWIDTH - CD_WadPick_GetTextWidth(title)) / 2, 10, title, 0xFF, 0xFF, 0xFF);
		
		//Present renderer
		SDL_RenderPresent(wadpick_renderer);
	}
	
	//Destroy WAD picker stuff
	SDL_DestroyTexture(wadpick_texture);
	SDL_DestroyRenderer(wadpick_renderer);
	SDL_DestroyWindow(wadpick_window);
	
	if (exit)
		I_Quit();
}
