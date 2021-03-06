
/*
 * draw a line between two given points in the current color.
 *
 * Called by:
 *     Cont_abs() in ../lib/Cont_abs.c
 */

#include <math.h>

#include "pngdriver.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

void PNG_draw_bitmap(int ncols, int nrows, int threshold,
		     const unsigned char *buf)
{
    int i0 = max(clip_left - cur_x, 0);
    int i1 = min(clip_rite - cur_x, ncols);
    int j0 = max(clip_top - cur_y, 0);
    int j1 = min(clip_bot - cur_y, nrows);

    if (!true_color) {
	int i, j;

	for (j = j0; j < j1; j++) {
	    int y = cur_y + j;

	    for (i = i0; i < i1; i++) {
		int x = cur_x + i;
		unsigned int k = buf[j * ncols + i];
		unsigned int *p = &grid[y * width + x];

		if (k > threshold)
		    *p = currentColor;
	    }
	}
    }
    else {
	int r1, g1, b1, a1;
	int i, j;

	get_pixel(currentColor, &r1, &g1, &b1, &a1);

	for (j = j0; j < j1; j++) {
	    int y = cur_y + j;

	    for (i = i0; i < i1; i++) {
		int x = cur_x + i;
		unsigned int k = buf[j * ncols + i];
		unsigned int *p = &grid[y * width + x];
		unsigned int a0, r0, g0, b0;
		unsigned int a, r, g, b;

		get_pixel(*p, &r0, &g0, &b0, &a0);

		a = (a0 * (255 - k) + a1 * k) / 255;
		r = (r0 * (255 - k) + r1 * k) / 255;
		g = (g0 * (255 - k) + g1 * k) / 255;
		b = (b0 * (255 - k) + b1 * k) / 255;

		*p = get_color(r, g, b, a);
	    }
	}
    }

    modified = 1;
}
