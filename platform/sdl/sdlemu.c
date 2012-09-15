/*
	sdlemu.c - based on platform/linux/gp2x.c

	(c) Copyright 2006 notaz, All rights reserved.
	Free for non-commercial use.

	For commercial use, separate licencing terms must be obtained.

*/

#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>

#include "sdlemu.h"

void *sdl_screen; // working buffer 320*230*2 + 320*2
SDL_Surface *screen;
SDL_Surface *menu_screen;
static int current_bpp = 8;
static int current_pal[256];

void sdl_init(void)
{
	printf("Initializing SDL... ");
	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) {
		printf("ERROR: %s.\n", SDL_GetError());
		return;
	}
	printf("Ok.\n");

	screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
	SDL_ShowCursor(0);

	sdl_screen = malloc(320*240*2 + 320*2);
	memset(sdl_screen, 0, 320*240*2 + 320*2);

	menu_screen = SDL_CreateRGBSurfaceFrom(sdl_screen, 320, 240, 16, 640, 0xF800, 0x7E0, 0x1F, 0);
}

char *ext_menu = 0, *ext_state = 0;

void sdl_deinit(void)
{
	SDL_FreeSurface(screen);
	SDL_Quit();
}

/* video */
void sdl_video_flip(void) // called from emu loop and menu loop
{
	int i;

	SDL_LockSurface(screen);
	if (current_bpp == 8)
	{
		unsigned short *fbp = (unsigned short *)screen->pixels;
		unsigned char *pixels = sdl_screen;

		for (i = 320*240; i--;)
		{
			fbp[i] = current_pal[pixels[i]];
		}
	}
	else
	{
		unsigned int *fbp = (unsigned int *)screen->pixels;
		unsigned int *pixels = sdl_screen;

		for (i = 320*240/2; i--;)
		{
			fbp[i] = pixels[i];
		}
	}
	SDL_UnlockSurface(screen);

	SDL_Flip(screen);
}

void sdl_menu_flip(void)
{
	SDL_Rect dst;

	dst.x = (screen->w - 320) / 2;
	dst.y = (screen->h - 240) / 2;

	SDL_BlitSurface(menu_screen, 0, screen, &dst);
	SDL_Flip(screen);
}

void sdl_video_changemode(int bpp)
{
	current_bpp = bpp;
	printf("BPP: %i\n", bpp);
}

void sdl_video_setpalette(int *pal, int len)
{
	memcpy(current_pal, pal, len*4);
}

void sdl_video_RGB_setscaling(int v_offs, int W, int H)
{
}

void sdl_memcpy_buffers(int buffers, void *data, int offset, int len)
{
	if ((char *)sdl_screen + offset != data)
		memcpy((char *)sdl_screen + offset, data, len);
}

void sdl_memcpy_all_buffers(void *data, int offset, int len)
{
	memcpy((char *)sdl_screen + offset, data, len);
}


void sdl_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)sdl_screen + offset, byte, len);
}

void sdl_pd_clone_buffer2(void)
{
	memset(sdl_screen, 0, 320*240*2);
}

/* sound */
#define SDL_BUFFER_SIZE 4096
static SDL_mutex *sound_mutex;
static SDL_cond *sound_cv;
static int sdl_sound_buffer[SDL_BUFFER_SIZE];
static int *sdl_sound_current_pos = NULL;
static int sdl_sound_current_emulated_samples = 0;

static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;
static int s_initialized = 0;

void sdl_sound_callback(void *userdata, Uint8 *stream, int len)
{
	SDL_LockMutex(sound_mutex);

	if(sdl_sound_current_emulated_samples < len/4) {
		memset(stream, 0, len);
	} else {
		memcpy(stream, sdl_sound_buffer, len);
		memmove(sdl_sound_buffer, sdl_sound_buffer + len/4, (sdl_sound_current_pos - sdl_sound_buffer)*4 - len);
		sdl_sound_current_pos -= len/4;
		sdl_sound_current_emulated_samples -= len/4;
	}

	SDL_CondSignal(sound_cv);
	SDL_UnlockMutex(sound_mutex);
}

void sdl_sound_volume(int l, int r)
{
}

void sdl_stop_sound()
{
	SDL_DestroyMutex(sound_mutex);
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	s_initialized = 0;
}

void sdl_start_sound(int rate, int bits, int stereo)
{
	SDL_AudioSpec as_desired, as_obtained;

	// if no settings change, we don't need to do anything
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo) return;
	if(s_initialized) sdl_stop_sound();

	as_desired.freq = rate;
	as_desired.format = AUDIO_S16; // `bits` is always 16
	as_desired.channels = stereo+1;
	as_desired.samples = 256;
	as_desired.callback = sdl_sound_callback;

	if(SDL_OpenAudio(&as_desired, &as_obtained) == -1) {
		printf("ERROR: can't open audio: %s.\n", SDL_GetError());
		return;
	}

	sound_mutex = SDL_CreateMutex();
	sound_cv = SDL_CreateCond(); 

	memset(sdl_sound_buffer, 0, SDL_BUFFER_SIZE);
	sdl_sound_current_pos = sdl_sound_buffer;
	sdl_sound_current_emulated_samples = 0;

	SDL_PauseAudio(0);
	s_initialized = 1; s_oldrate = rate; s_oldbits = bits; s_oldstereo = stereo;
}

void sdl_sound_write(void *buff, int len)
{
	int i, *src = (int *)buff;

	SDL_LockMutex(sound_mutex);

	for(i = 0; i < (len >> 2); i++) {
		while(sdl_sound_current_emulated_samples > SDL_BUFFER_SIZE*3/4) SDL_CondWait(sound_cv, sound_mutex);
		*sdl_sound_current_pos++ = src[i];
		sdl_sound_current_emulated_samples += 1;
	}

	SDL_CondSignal(sound_cv);
	SDL_UnlockMutex(sound_mutex);
}

/* joystick emulation */
#define SETKEY(KEY, BUT) \
{ \
	if(event.key.keysym.sym == (KEY)) { \
		if(event.type == SDL_KEYUP) {\
			button_states &= ~(BUT);\
		} else if (event.type == SDL_KEYDOWN) {\
			button_states &= ~(BUT);\
			button_states |= (BUT);\
		} \
	}\
}

static unsigned long button_states = 0;
unsigned long sdl_joystick_read(int allow_usb_joy)
{
	SDL_Event event;

	if(SDL_PollEvent(&event)) {
		if(event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) {
			SETKEY(SDLK_UP, GP2X_UP);
			SETKEY(SDLK_DOWN, GP2X_DOWN);
			SETKEY(SDLK_LEFT, GP2X_LEFT);
			SETKEY(SDLK_RIGHT, GP2X_RIGHT);
			SETKEY(SDLK_LCTRL, GP2X_B);
			SETKEY(SDLK_LALT, GP2X_X);
			SETKEY(SDLK_SPACE, GP2X_Y);
			SETKEY(SDLK_LSHIFT, GP2X_A);
			SETKEY(SDLK_TAB, GP2X_L);
			SETKEY(SDLK_BACKSPACE, GP2X_R);
			SETKEY(SDLK_ESCAPE, GP2X_SELECT);
			SETKEY(SDLK_RETURN, GP2X_START);
		}
	}

	return button_states;
}


/* misc */
void spend_cycles(int c)
{
	usleep(c/200);
}

/* lprintf stub */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}