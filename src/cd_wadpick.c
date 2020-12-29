#include "SDL.h"

#include <string.h>
#include <unistd.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "d_main.h"

typedef struct
{
	const char *name;
	GameMode_t gamemode;
	Language_t language;
} wadchk_t;

const wadchk_t wadchk[] = {
	{"doom.wad", registered, english},
	{"doom1.wad", shareware, english},
	{"doomu.wad", retail, english},
	{"doom2.wad", commercial, english},
	{"doom2f.wad", commercial, french},
	{"plutonia.wad", commercial, english},
	{"tnt.wad", commercial, english},
	{NULL, indetermined, english},
};

void CD_PickWad(const wadchk_t *wad)
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
	
	if (wadpick_opts == 1)
	{
		//Found 1 WAD, no point in picking
		CD_PickWad(wadpick_opt[0]);
		return;
	}
	else if (wadpick_opts == 0)
	{
		//Failed to find any WADs, no point in picking
		return;
	}
	
	//Ensure SDL2 is initialized
	if (!SDL_WasInit(SDL_INIT_EVERYTHING))
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
			I_Error((char*)SDL_GetError());
	
	//Create WAD picker window and renderer
	SDL_Window *window = SDL_CreateWindow("CuckyDOOM - WAD Picker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREENWIDTH, SCREENHEIGHT, 0);
	if (window == NULL)
		I_Error((char*)SDL_GetError());
	
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
	if (renderer == NULL)
		I_Error((char*)SDL_GetError());
	
	//Destroy WAD picker window and renderer
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
