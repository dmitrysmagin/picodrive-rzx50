
#include <stdint.h>

#include "scaler.h"

extern int current_pal[256];

#define AVERAGE(z, x) ((((z) & 0xF7DEF7DE) >> 1) + (((x) & 0xF7DEF7DE) >> 1))
#define AVERAGEHI(AB) ((((AB) & 0xF7DE0000) >> 1) + (((AB) & 0xF7DE) << 15))
#define AVERAGELO(CD) ((((CD) & 0xF7DE) >> 1) + (((CD) & 0xF7DE0000) >> 17))

/*
	Upscale 320x224 -> 480x272

	Horizontal interpolation
		480/320=1.5
		4p -> 6p
		2dw -> 3dw

		for each line: 4 pixels => 6 pixels (*1.5) (80 blocks)
		[ab][cd] => [a(ab)][bc][(cd)d]

	Vertical upscale:
		Bresenham algo with simple interpolation

	Parameters:
	uint32_t *dst - pointer to 480x272x16bpp buffer
	uint8_t *src - pointer to 320x224x8bpp buffer
	palette is taken from current_pal[]
	pitch correction

*/

void upscale_320x224x8_to_480x272(uint32_t *dst, uint8_t *src)
{
	int midh = 272 * 3 / 4;
	int Eh = 0;
	int source = 0;
	int dh = 0;
	int y, x;

	src += 320*8; // skip 8 upper lines

	for (y = 0; y < 272; y++)
	{
		source = dh * 320;

		for (x = 0; x < 480/6; x++)
		{
			register uint32_t ab, cd;

			__builtin_prefetch(dst + 4, 1);
			__builtin_prefetch(src + source + 4, 0);

			ab = current_pal[*(uint16_t *)(src + source)] & 0xF7DEF7DE;
			cd = current_pal[*(uint16_t *)(src + source + 2)] & 0xF7DEF7DE;

			if(Eh >= midh) { // average + 256
				ab = AVERAGE(ab, current_pal[*(uint16_t *)(src + source + 320)]) & 0xF7DEF7DE; // to prevent overflow
				cd = AVERAGE(cd, current_pal[*(uint16_t *)(src + source + 320 + 2)]) & 0xF7DEF7DE; // to prevent overflow
			}

			*dst++ = (ab & 0xFFFF) + AVERAGEHI(ab);
			*dst++ = (ab >> 16) + ((cd & 0xFFFF) << 16);
			*dst++ = (cd & 0xFFFF0000) + AVERAGELO(cd);

			source += 4;

		}

		Eh += 232; if(Eh >= 272) { Eh -= 272; dh++; }
	}

}
 