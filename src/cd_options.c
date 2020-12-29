#include "cd_options.h"

#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "doomstat.h"
#include "dstrings.h"
#include "sounds.h"
#include "r_main.h"

//Option externs
extern long showMessages;

extern long mouseSensitivity;
extern long mouseMove;

extern long snd_SfxVolume;
extern long snd_MusicVolume;

extern long screenSize;
extern long screenblocks;
extern long detailLevel;

//Option change functions
extern boolean message_dontfuckwithme;

void CD_ChangeShowMessages()
{
	if (!showMessages)
		players[consoleplayer].message = MSGOFF;
	else
		players[consoleplayer].message = MSGON ;
	message_dontfuckwithme = true;
}

void CD_ChangeScreenSize()
{
	screenblocks = screenSize + 3;
	R_SetViewSize (screenblocks, detailLevel);
}

void CD_ChangeGamma()
{
	I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

//Options
typedef enum
{
	optiontype_bool,
	optiontype_int,
	optiontype_string,
	optiontype_label,
} optiontype_t;

typedef struct
{
	const char *name;
	long *value;
	void (*change_proc)();
	optiontype_t type;
	union
	{
		struct
		{
			byte dummy;
		} type_bool;
		struct
		{
			int min, max;
		} type_int;
		struct
		{
			const char **title;
		} type_string;
		struct
		{
			byte dummy;
		} type_label;
	};
} option_t;

const option_t options[] = {
	{"Gameplay",          NULL,              NULL,                  optiontype_label,  .type_label =  {0}},
	{"Messages",          &showMessages,     CD_ChangeShowMessages, optiontype_bool,   .type_bool =   {0}},
	
	{"Video",             NULL,              NULL,                  optiontype_label,  .type_label =  {0}},
	{"Screen Size",       &screenSize,       CD_ChangeScreenSize,   optiontype_int,    .type_int =    {0, 8}},
	{"Gamma",             &usegamma,         CD_ChangeGamma,        optiontype_int,    .type_int =    {0, 4}},
	
	{"Input",             NULL,              NULL,                  optiontype_label,  .type_label =  {0}},
	{"Mouse Sensitivity", &mouseSensitivity, NULL,                  optiontype_int,    .type_int =    {0, 15}},
	{"Mouse Move",        &mouseMove,        NULL,                  optiontype_bool,   .type_bool =   {0}},
	
	{"Sound",             NULL,              NULL,                  optiontype_label,  .type_label =  {0}},
	{"Sound Volume",      &snd_SfxVolume,    NULL,                  optiontype_int,    .type_int =    {0, 15}},
	{"Music Volume",      &snd_MusicVolume,  NULL,                  optiontype_int,    .type_int =    {0, 15}},
	
	{NULL,                NULL,              NULL,                  optiontype_label,  .type_label = {0}},
};

int option_select;
int option_scroll;

extern boolean menuactive;

extern short whichSkull;
extern char skullName[2][9];

void CD_OptionsStart()
{
	option_select = 0;
	option_scroll = 0;
	while (options[option_select].value == NULL)
		option_select++;
}

boolean CD_OptionsResponder(int ch)
{
	const option_t *option;
	
	switch (ch)
	{
		case KEY_UPARROW:
			do
			{
				if (--option_select < 0)
				{
					option_select = 0;
					while (options[option_select].value == NULL)
						option_select++;
				}
			} while (options[option_select].value == NULL);
			S_StartSound(NULL,sfx_pstop);
			break;
		case KEY_DOWNARROW:
			do
			{
				if (options[++option_select].name == NULL)
				{
					do
						option_select--;
					while (options[option_select].value == NULL);
				}
			} while (options[option_select].value == NULL);
			S_StartSound(NULL,sfx_pstop);
			break;
		case KEY_LEFTARROW:
			option = &options[option_select];
			switch (option->type)
			{
				case optiontype_bool:
					*option->value = *option->value ? false : true;
					break;
				case optiontype_int:
					if (--*option->value < option->type_int.min)
						*option->value = option->type_int.min;
					break;
			}
			if (option->change_proc != NULL)
				option->change_proc();
			S_StartSound(NULL,sfx_stnmov);
			return true;
		case KEY_ENTER:
		case KEY_RIGHTARROW:
			option = &options[option_select];
			switch (option->type)
			{
				case optiontype_bool:
					*option->value = *option->value ? false : true;
					break;
				case optiontype_int:
					if (++*option->value > option->type_int.max)
						*option->value = option->type_int.max;
					break;
			}
			if (option->change_proc != NULL)
				option->change_proc();
			S_StartSound(NULL,sfx_stnmov);
			return true;
		case KEY_ESCAPE:
			menuactive = false;
			S_StartSound(NULL, sfx_swtchn);
			return true;
		case KEY_BACKSPACE:
			menuactive = false;
			M_StartControlPanel();
			S_StartSound(NULL, sfx_swtchn);
			return true;
		default:
			break;
	}
	return false;
}

void CD_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int		xx;
    int		i;

	xx = x;
	
	//Draw thermometer
	V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERML",PU_CACHE));
	xx += 8;
	
	for (i = 0; i < 9; i++)
	{
		V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMM",PU_CACHE));
		xx += 8;
	}
	
	V_DrawPatchDirect (xx, y, 0, W_CacheLumpName("M_THERMR",PU_CACHE));
	
	//Draw notch
	x += 8;
	xx -= 8;
	V_DrawPatchDirect (x + (xx - x) * thermDot / thermWidth, y,
			   0,W_CacheLumpName("M_THERMO",PU_CACHE));
}

void CD_OptionsDrawer()
{
	//Draw "OPTIONS" title
	V_DrawPatchDirect (108,15,0,W_CacheLumpName("M_OPTTTL",PU_CACHE));
	
	//Draw options
	int tgt_scroll;
	int max_scroll;
	int y = 38;
	int i = 0;
	
	for (const option_t *option = options; option->name != NULL; option++, i++)
	{
		//Handle scrolling
		if (i == option_select)
		{
			tgt_scroll = y - 90;
			if (tgt_scroll < 0)
				tgt_scroll = 0;
		}
		
		//Handle drawing
		if ((y - option_scroll) >= 34 && (y - option_scroll) <= 155)
		{
			//Draw skull selector
			if (i == option_select)
				V_DrawPatchDirect(20, y - 5 - option_scroll, 0, W_CacheLumpName(skullName[whichSkull],PU_CACHE));
			
			//Draw option based off type
			switch (option->type)
			{
				case optiontype_bool:
					M_DrawText(48, y - option_scroll, 1, (char*)option->name);
					M_DrawText(220, y - option_scroll, 1, *option->value ? "On" : "Off");
					break;
				case optiontype_int:
					M_DrawText(48, y + 2 - option_scroll, 1, (char*)option->name);
					CD_DrawThermo(180, y - option_scroll, option->type_int.max - option->type_int.min, *option->value - option->type_int.min);
					break;
				case optiontype_label:
					M_DrawText(64, y + 2 - option_scroll, 1, (char*)option->name);
					break;
			}
		}
		
		//Increment Y position
		switch (option->type)
		{
			case optiontype_int:
				y += 14;
				break;
			case optiontype_label:
				y += 13;
				break;
			default:
				y += 10;
				break;
		}
		max_scroll = y - 155;
	}
	
	//Scroll
	if (tgt_scroll > max_scroll)
		tgt_scroll = max_scroll;
	option_scroll += (tgt_scroll - option_scroll) / 2;
}
