/*  The following code is the implementation of the thinning
   algorithm described in the "Analysis of Thinning Algorithms Using
   Mathematical Morphology" by Ben-Kwei Jang and Ronald T. Chin
   (Transactions on pattern analysis and machine intellegence, vol. 12,
   NO 6, JUNE 1990)  

   Olga Waupotitsch, USA CERL, jan, 1993

   The code for finding the bounding box as well as input/output code
   is written by Mike Baba (1990) (DBA Systems) and Jean Ezell (1988,
   USA CERL)
 */
/*  algorithm B */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include "local_proto.h"

#define LEFT 1
#define RIGHT 2

#define true 1
#define false 0
#define DELETED_PIX   9999

extern char *error_prefix;
static char *work_file_name;
static int n_rows, n_cols, pad_size;
static int box_right, box_left, box_top, box_bottom;

extern CELL *get_a_row(int row);
extern int put_a_row(int row, CELL * buf);

int thin_lines(int iterations)
{
    int j, i, col, deleted, row;
    CELL *row_buf, *new_med, *med, *bottom, *top, *get_a_row();
    char W, N_W, Templ[8], N_Templ[8];

    map_size(&n_rows, &n_cols, &pad_size);
    box_right = box_bottom = 0;
    box_left = n_cols;
    box_top = n_rows;
    bottom = get_a_row(pad_size - 1);
    for (row = pad_size; row < n_rows - pad_size; row++) {
	top = bottom;		/* line above the one we're changing */
	bottom = get_a_row(row);	/* line we're working on now */
	for (col = pad_size; col < n_cols - pad_size; col++) {
	    if (bottom[col]) {	/* not background pixel */
		if (col < box_left)	/* find bounding box which will */
		    box_left = col;	/*   cover part of raster map which */
		if (col > box_right)	/*   has lines in it */
		    box_right = col;
		if (row < box_top)
		    box_top = row;
		if (row > box_bottom)
		    box_bottom = row;
	    }
	}			/* col-loop */
	put_a_row(row, bottom);
    }				/* row-loop */
    if (box_right < box_left || box_bottom < box_top) {
	unlink(work_file_name);
	G_fatal_error(_("%s: Unable to find bounding box for lines"),
		      error_prefix);
    }
    G_message(_("Bounding box:  l = %d, r = %d, t = %d, b = %d"),
	      box_left, box_right, box_top, box_bottom);

    /*
     * thin - thin lines to a single pixel width
     *
     * (destructive)
     *
     *   +---+---+---+---+---+---+---+
     *   |   |   |   |   |   |   |   |
     *   +---+---+---+---+---+---+---+
     *   |   |   | t | t | t |   |   |   row - 1
     *   +---+---+---+---+---+---+---+
     *   |   |   | t | T | t |   |   |   row
     *   +---+---+---+---+---+---+---+
     *   |   |   | t | t | t |   |   |   row + 1
     *   +---+---+---+---+---+---+---+
     *   |   |   |   |   |   |   |   |
     *   +---+---+---+---+---+---+---+
     *                col
     */
    Templ[0] = /* 00101000 */ 40;
    Templ[1] = /* 00001010 */ 10;
    Templ[2] = /* 10000010 */ 130;
    Templ[3] = /* 10100000 */ 160;
    Templ[4] = /* 00101010 */ 42;
    Templ[5] = /* 10001010 */ 138;
    Templ[6] = /* 10100010 */ 162;
    Templ[7] = /* 10101000 */ 168;

    N_Templ[0] = /* 10000011 */ 131;
    N_Templ[1] = /* 11100000 */ 224;
    N_Templ[2] = /* 00111000 */ 56;
    N_Templ[3] = /* 00001110 */ 14;
    N_Templ[4] = /* 10000000 */ 128;
    N_Templ[5] = /* 00100000 */ 32;
    N_Templ[6] = /* 00001000 */ 8;
    N_Templ[7] = /* 00000010 */ 2;

    new_med = (CELL *) G_malloc(sizeof(CELL) * (n_cols));
    for (i = 0; i < n_cols; i++)
	new_med[i] = 0;
    row_buf = (CELL *) G_malloc(sizeof(CELL) * (n_cols));
    for (i = 0; i < n_cols; i++)
	row_buf[i] = 0;

    deleted = 1;
    i = 1;
    while ((deleted > 0) && (i <= iterations)) {	/* it must be done in <= iterations passes */
	G_message(_("Pass number %d"), i);
	i++;
	deleted = 0;
	for (j = 1; j <= 4; j++) {
	    int ind1, ind2, ind3;

	    ind1 = j - 1;
	    if (j <= 3)
		ind2 = j;
	    else
		ind2 = 0;
	    ind3 = (j - 1) + 4;

	    top = get_a_row(box_top - 1);
	    med = get_a_row(box_top);

	    for (row = box_top; row <= box_bottom; row++) {
		for (col = box_left; col <= box_right; col++)
		    new_med[col] = med[col];
		bottom = get_a_row(row + 1);
		for (col = box_left; col <= box_right; col++) {
		    if (med[col]) {	/* if pixel is not blank */
			W = encode_neighbours(top, med, bottom, col, 1);
			/* current window */
			N_W = encode_neighbours(top, med, bottom, col, -1);
			/* negated window */
			if ((((Templ[ind1] & W) == Templ[ind1]) &&
			     ((N_Templ[ind1] & N_W) == N_Templ[ind1])) ||
			    (((Templ[ind2] & W) == Templ[ind2]) &&
			     ((N_Templ[ind2] & N_W) == N_Templ[ind2])) ||
			    (((Templ[ind3] & W) == Templ[ind3]) &&
			     ((N_Templ[ind3] & N_W) == N_Templ[ind3]))) {
			    /* fprintf(stdout, "col: %d,   row: %d\n", col, row); */
			    deleted++;
			    new_med[col] = 0;
			}

		    }		/* end blank pixel */
		}		/* end col loop */

		for (col = box_left; col <= box_right; col++)
		    row_buf[col] = med[col];
		top = row_buf;
		put_a_row(row, new_med);
		/* this is because I want to keep the old copy op top */
		/* if I say top=med,  top will point to already changed */
		/* row, since put_a_row(row, med_row) was called and med */
		med = bottom;
	    }			/* end row loop */
	}			/* j-loop */

	G_message(_("Deleted %d  pixels "), deleted);
    }				/* while delete >0 */

    if ((deleted == 0) && (i <= iterations))
	G_message(_("Thinning completed successfully."));
    else
	G_message(_("Thinning not completed, consider to increase 'iterations' parameter."));

    return 0;
}


/* encode_neighbours- return neighborhood information for pixel at (middle,col) */

char
encode_neighbours(CELL * top, CELL * middle, CELL * bottom, int col, int neg)
{
    char T;

    T = 0;
    if (neg > 0)
	T = (((middle[col + 1] != 0) << 5) | ((top[col + 1] !=
					       0) << 6) | ((top[col] !=
							    0) << 7) |
	     ((top[col - 1] != 0)) | ((middle[col - 1] !=
				       0) << 1) | ((bottom[col - 1] !=
						    0) << 2) | ((bottom[col]
								 !=
								 0) << 3) |
	     ((bottom[col + 1] != 0) << 4));
    else
	T = (((middle[col + 1] == 0) << 5) | ((top[col + 1] ==
					       0) << 6) | ((top[col] ==
							    0) << 7) |
	     ((top[col - 1] == 0)) | ((middle[col - 1] ==
				       0) << 1) | ((bottom[col - 1] ==
						    0) << 2) | ((bottom[col]
								 ==
								 0) << 3) |
	     ((bottom[col + 1] == 0) << 4));

    return (T);
}
