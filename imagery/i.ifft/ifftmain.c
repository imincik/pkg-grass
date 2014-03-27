
/****************************************************************************
 *
 * MODULE:       i.ifft
 * AUTHOR(S):    David B. Satnik and Ali R. Vali (original contributors),
 *               Markus Neteler <neteler itc.it>
 *               Bernhard Reiter <bernhard intevation.de>, 
 *               Brad Douglas <rez touchofmadness.com>, 
 *               Glynn Clements <glynn gclements.plus.com>
 * PURPOSE:      processes the real and imaginary Fourier
 *               components in frequency space and constract raster map
 * COPYRIGHT:    (C) 1999-2008 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/
/*
   Central Washington University GIS Laboratory
   Programmer: David B. Satnik
   and
   Programmer : Ali R. Vali
   Center for Space Research
   WRW 402
   University of Texas
   Austin, TX 78712-1085

   (512) 471-6824

 */

#define MAIN

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include <grass/gmath.h>
#include "globals.h"
#include "local_proto.h"


int main(int argc, char *argv[])
{
    /* Global variable & function declarations */
    char Cellmap_orig[50];
    FILE *realfp, *imagfp;	/* the input and output file descriptors */
    int outputfd, maskfd;	/* the input and output file descriptors */
    char *realmapset, *imagmapset;	/* the input mapset names */
    struct Cell_head orig_wind, realhead;
    CELL *cell_row, *maskbuf = NULL;

    int i, j;			/* Loop control variables */
    int or, oc;			/* Original dimensions of image */
    int rows, cols;		/* Smallest powers of 2 >= number of rows & columns */
    long totsize;		/* Total number of data points */
    int halfrows, halfcols;
    double *data[2];		/* Data structure containing real & complex values of FFT */
    struct Option *op1, *op2, *op3;
    struct GModule *module;

    G_gisinit(argv[0]);

    /* Set description */
    module = G_define_module();
    module->keywords = _("imagery, FFT");
    module->description =
	_("Inverse Fast Fourier Transform (IFFT) for image processing.");

    /* define options */
    op1 = G_define_standard_option(G_OPT_R_INPUT);
    op1->key = "real_image";
    op1->description = _("Name of input raster map (image fft, real part)");

    op2 = G_define_standard_option(G_OPT_R_INPUT);
    op2->key = "imaginary_image";
    op2->description = _("Name of input raster map (image fft, imaginary part");

    op3 = G_define_standard_option(G_OPT_R_OUTPUT);
    op3->key = "output_image";
    op3->description = _("Name for output raster map");

    /*call parser */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    strcpy(Cellmap_real, op1->answer);
    strcpy(Cellmap_imag, op2->answer);
    strcpy(Cellmap_orig, op3->answer);

    /* open input raster map */
    if ((realmapset = G_find_cell(Cellmap_real, "")) == NULL)
	G_fatal_error(_("Raster map <%s> not found"),
		      Cellmap_real);

    if ((realfp =
	 G_fopen_old_misc("cell_misc", "fftreal", Cellmap_real,
			  realmapset)) == NULL)
	G_fatal_error(_("Unable to open real-image in the 'cell_misc' directory. "
			"Raster map probably wasn't created by i.fft"));

    if ((imagmapset = G_find_cell(Cellmap_imag, "")) == NULL)
	G_fatal_error(_("Raster map <%s> not found"),
		      Cellmap_imag);

    if ((imagfp =
	 G_fopen_old_misc("cell_misc", "fftimag", Cellmap_imag,
			  imagmapset)) == NULL)
	G_fatal_error(_("Unable to open imaginary-image in the 'cell_misc' directory. "
			"Raster map probably wasn't created by i.fft"));

    /* check command line args for validity */
    if (G_legal_filename(Cellmap_orig) < 0)
	G_fatal_error(_("<%s> is an illegal file name"),
		      Cellmap_orig);

    /* get and compare the original window data */
    get_orig_window(&orig_wind, realmapset, imagmapset);

    or = orig_wind.rows;
    oc = orig_wind.cols;
    G_get_cellhd(Cellmap_real, realmapset, &realhead);
    G_set_window(&realhead);	/* set the window to the whole cell map */

    /* get the rows and columns in the current window */
    rows = G_window_rows();
    cols = G_window_cols();
    totsize = rows * cols;
    halfrows = rows / 2;
    halfcols = cols / 2;

    G_verbose_message(_("Power 2 values: %d rows %d columns"), rows, cols);

    /* Allocate appropriate memory for the structure containing
       the real and complex components of the FFT.  DATA[0] will
       contain the real, and DATA[1] the complex component.
     */
    data[0] = (double *)G_malloc((rows * cols) * sizeof(double));
    data[1] = (double *)G_malloc((rows * cols) * sizeof(double));

    /* Initialize real & complex components to zero */
    G_message(_("Reading raster maps..."));
    {
	fread((char *)data[0], sizeof(double), totsize, realfp);
	fread((char *)data[1], sizeof(double), totsize, imagfp);
    }

    /* Read in cell map values */
    G_message(_("Masking raster maps..."));
    maskfd = G_maskfd();
    if (maskfd >= 0)
	maskbuf = G_allocate_cell_buf();

    if (maskfd >= 0) {
	for (i = 0; i < rows; i++) {
	    double *data0, *data1;

	    data0 = data[0] + i * cols;
	    data1 = data[1] + i * cols;
	    G_get_map_row(maskfd, maskbuf, i);
	    for (j = 0; j < cols; j++, data0++, data1++) {
		if (maskbuf[j] == (CELL) 0) {
		    *(data0) = 0.0;
		    *(data1) = 0.0;
		}
	    }
	}
    }

    G_message(_("Rotating data..."));
    /* rotate the data array for standard display */
    for (i = 0; i < rows; i++) {
	double temp;

	for (j = 0; j < halfcols; j++) {
	    temp = *(data[0] + i * cols + j);
	    *(data[0] + i * cols + j) = *(data[0] + i * cols + j + halfcols);
	    *(data[0] + i * cols + j + halfcols) = temp;
	    temp = *(data[1] + i * cols + j);
	    *(data[1] + i * cols + j) = *(data[1] + i * cols + j + halfcols);
	    *(data[1] + i * cols + j + halfcols) = temp;
	}
    }
    for (i = 0; i < halfrows; i++) {
	double temp;

	for (j = 0; j < cols; j++) {
	    temp = *(data[0] + i * cols + j);
	    *(data[0] + i * cols + j) =
		*(data[0] + (i + halfrows) * cols + j);
	    *(data[0] + (i + halfrows) * cols + j) = temp;
	    temp = *(data[1] + i * cols + j);
	    *(data[1] + i * cols + j) =
		*(data[1] + (i + halfrows) * cols + j);
	    *(data[1] + (i + halfrows) * cols + j) = temp;
	}
    }


    /* close input cell maps and release the row buffers */
    fclose(realfp);
    fclose(imagfp);
    if (maskfd >= 0) {
	G_close_cell(maskfd);
	G_free(maskbuf);
    }

    /* perform inverse FFT */
    G_message(_("Starting Inverse FFT..."));
    fft(1, data, totsize, cols, rows);

    /* set up a window for the transform cell map */
    G_set_window(&orig_wind);

    /* open the output cell map and allocate a cell row buffer */
    if ((outputfd = G_open_cell_new(Cellmap_orig)) < 0)
	G_fatal_error(_("Unable to create raster map <%s>"),
		      Cellmap_orig);

    cell_row = G_allocate_cell_buf();

    /* Write out result to a new cell map */
    G_message(_("Writing data..."));
    for (i = 0; i < or; i++) {
	for (j = 0; j < oc; j++) {
	    *(cell_row + j) = (CELL) (*(data[0] + i * cols + j) + 0.5);
	}
	G_put_raster_row(outputfd, cell_row, CELL_TYPE);

	G_percent(i+1, or, 2);
    }
    G_close_cell(outputfd);

    G_free(cell_row);
    {
	struct Colors colors;
	struct Range range;
	CELL min, max;

	/* make a real component color table */
	G_read_range(Cellmap_orig, G_mapset(), &range);
	G_get_range_min_max(&range, &min, &max);
	G_make_grey_scale_colors(&colors, min, max);
	G_write_colors(Cellmap_orig, G_mapset(), &colors);
    }

    /* Release memory resources */
    G_free(data[0]);
    G_free(data[1]);

    G_done_msg(" ");

    exit(EXIT_SUCCESS);
}
