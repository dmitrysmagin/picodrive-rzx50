/*
	main.c - based on platform/gp2x/main.c

	(c) Copyright 2006 notaz, All rights reserved.
	Free for non-commercial use.

	For commercial use, separate licencing terms must be obtained.

*/

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "sdlemu.h"
#include "menu.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "emu.h"
#include "version.h"

char *ext_menu, *ext_state;
extern int select_exits;
extern char *PicoConfigFile;

char **g_argv;

void parse_cmd_line(int argc, char *argv[])
{
	int x, unrecognized = 0;

	for(x = 1; x < argc; x++)
	{
		if(argv[x][0] == '-')
		{
			if(strcasecmp(argv[x], "-menu") == 0) {
				if(x+1 < argc) { ++x; ext_menu = argv[x]; } /* External Frontend: Program Name */
			}
			else if(strcasecmp(argv[x], "-state") == 0) {
				if(x+1 < argc) { ++x; ext_state = argv[x]; } /* External Frontend: Arguments */
			}
			else if(strcasecmp(argv[x], "-config") == 0) {
				if(x+1 < argc) { ++x; PicoConfigFile = argv[x]; }
			}
			else if(strcasecmp(argv[x], "-selectexit") == 0) {
				select_exits = 1;
			}
			else {
				unrecognized = 1;
				break;
			}
		} else {
			/* External Frontend: ROM Name */
			FILE *f;
			strncpy(romFileName, argv[x], PATH_MAX);
			romFileName[PATH_MAX-1] = 0;
			f = fopen(romFileName, "rb");
			if (f) fclose(f);
			else unrecognized = 1;
			engineState = PGS_ReloadRom;
			break;
		}
	}

	if (unrecognized) {
		printf("\n\n\nPicoDrive SDL v0.1 (c) exmortis, 2012\n");
		printf("Based on PicoDrive v" VERSION " (c) notaz, 2006-2007\n");
		printf("usage: %s [options] [romfile]\n", argv[0]);
/*
		printf( "options:\n"
				"-menu <menu_path> launch a custom program on exit instead of default gmenu2x\n"
				"-state <param>    pass '-state param' to the menu program\n"
				"-config <file>    use specified config file instead of default 'picoconfig.bin'\n"
				"                  see currentConfig_t structure in emu.h for the file format\n"
				"-selectexit       pressing SELECT will exit the emu and start 'menu_path'\n");
*/
	}
}

#undef main

int main(int argc, char *argv[])
{
	g_argv = argv;

	emu_ReadConfig(0, 0);
	sdl_init();

	emu_Init();
	menu_init();

	engineState = PGS_Menu;

	if (argc > 1)
		parse_cmd_line(argc, argv);

	for (;;)
	{
		switch (engineState)
		{
			case PGS_Menu:
				menu_loop();
				break;

			case PGS_ReloadRom:
				if (emu_ReloadRom())
					engineState = PGS_Running;
				else {
					printf("PGS_ReloadRom == 0\n");
					engineState = PGS_Menu;
				}
				break;

			case PGS_RestartRun:
				engineState = PGS_Running;

			case PGS_Running:
				emu_Loop();
				break;

			case PGS_Quit:
				goto endloop;

			default:
				printf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	emu_Deinit();
	sdl_deinit();

	return 0;
}
