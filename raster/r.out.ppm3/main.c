
/****************************************************************************
 *
 * MODULE:       r.out.ppm3
 * AUTHOR(S):    Glynn Clements <glynn gclements.plus.com> (original contributor)
 *               Jachym Cepicky <jachym les-ejk.cz>, Markus Neteler <neteler itc.it>
 * PURPOSE:      Use to convert 3 grass raster layers (R,G,B) to PPM
 *               uses currently selected region
 * COPYRIGHT:    (C) 2001-2006 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include <grass/gis.h>
#include <grass/glocale.h>

#define DEF_RED 255
#define DEF_GRN 255
#define DEF_BLU 255

struct band
{
    struct Option *opt;
    int file;
    int type;
    void *array;
    struct Colors colors;
    unsigned char *buf;
    unsigned char *mask;
};

static char *const color_names[3] = { "red", "green", "blue" };

int main(int argc, char **argv)
{
    struct band B[3];
    struct GModule *module;
    struct Option *ppm_file;

    /* please, remove before GRASS 7 released */
    struct Flag *bequiet, *comment;
    struct Cell_head w;
    FILE *fp;
    unsigned char *dummy;
    int row, col;
    int i;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, export");
    module->description =
	_("Converts 3 GRASS raster layers (R,G,B) to a PPM image file "
	  "at the pixel resolution of the CURRENTLY DEFINED REGION.");

    for (i = 0; i < 3; i++) {
	char buff[80];

	sprintf(buff, _("Name of raster map to be used for <%s>"),
		color_names[i]);

	B[i].opt = G_define_option();
	B[i].opt->key = G_store(color_names[i]);
	B[i].opt->type = TYPE_STRING;
	B[i].opt->answer = NULL;
	B[i].opt->required = YES;
	B[i].opt->multiple = NO;
	B[i].opt->gisprompt = "old,cell,raster";
	B[i].opt->description = G_store(buff);
    }

    ppm_file = G_define_option();
    ppm_file->key = "output";
    ppm_file->type = TYPE_STRING;
    ppm_file->required = YES;
    ppm_file->multiple = NO;
    ppm_file->answer = NULL;
    ppm_file->description =
	_("Name for new PPM file. (use out=- for stdout)");

    /* please, remove before GRASS 7 released */
    bequiet = G_define_flag();
    bequiet->key = 'q';
    bequiet->description = _("Run quietly");

    comment = G_define_flag();
    comment->key = 'c';
    comment->description = _("Add comments to describe the region");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* please, remove before GRASS 7 released */
    if (bequiet->answer) {
	putenv("GRASS_VERBOSE=0");
	G_warning(_("The '-q' flag is superseded and will be removed "
		    "in future. Please use '--quiet' instead."));
    }


    G_get_window(&w);

    G_message(_("rows = %d, cols = %d"), w.rows, w.cols);

    /* open raster map for reading */
    for (i = 0; i < 3; i++) {
	/* Get name of layer */
	char *name = B[i].opt->answer;
	char *mapset;

	/* Get mapset of layer */
	mapset = G_find_cell2(name, "");
	if (!mapset)
	    G_fatal_error(_("Raster map <%s> not found"), name);

	/* Open raster map */
	if ((B[i].file = G_open_cell_old(name, mapset)) == -1)
	    G_fatal_error(_("Unable to open raster map <%s>"), name);

	/* Get map type (CELL/FCELL/DCELL) */
	B[i].type = G_get_raster_map_type(B[i].file);

	/* Get color table */
	if (G_read_colors(name, mapset, &B[i].colors) == -1)
	    G_fatal_error(_("Color file for <%s> not available"), name);

	/* Allocate input buffer */
	B[i].array = G_allocate_raster_buf(B[i].type);

	/* Allocate output buffers */
	B[i].buf = (unsigned char *)G_malloc(w.cols);
	B[i].mask = (unsigned char *)G_malloc(w.cols);
    }

    dummy = (unsigned char *)G_malloc(w.cols);

    /* open PPM file for writing */
    if (strcmp(ppm_file->answer, "-") == 0)
	fp = stdout;
    else {
	fp = fopen(ppm_file->answer, "w");
	if (!fp)
	    G_fatal_error(_("Unable to open file <%s>"), ppm_file->answer);
    }

    /* write header info */

    /* Magic number meaning rawbits, 24bit color to PPM format */
    fprintf(fp, "P6\n");

    /* comments */
    if (comment->answer) {
	fprintf(fp, "# CREATOR: r.out.ppm3 (from GRASS)\n");
	fprintf(fp, "# Red:   %s\n", B[0].opt->answer);
	fprintf(fp, "# Green: %s\n", B[1].opt->answer);
	fprintf(fp, "# Blue:  %s\n", B[2].opt->answer);
	fprintf(fp, "# Projection: %s (Zone: %d)\n",
		G_database_projection_name(), G_zone());
	fprintf(fp, "# N=%f, S=%f, E=%f, W=%f\n",
		w.north, w.south, w.east, w.west);
	fprintf(fp, "# N/S Res: %f, E/W Res: %f\n", w.ns_res, w.ew_res);
    }

    /* width & height */
    fprintf(fp, "%d %d\n", w.cols, w.rows);

    /* max intensity val */
    fprintf(fp, "255\n");

    G_message(_("Converting ... "));

    for (row = 0; row < w.rows; row++) {
	G_percent(row, w.rows, 5);

	for (i = 0; i < 3; i++) {
	    if (G_get_raster_row(B[i].file, B[i].array, row, B[i].type) < 0)
		G_fatal_error("G_get_raster_row failed");

	    G_lookup_raster_colors(B[i].array,
				   (i == 0) ? B[i].buf : dummy,
				   (i == 1) ? B[i].buf : dummy,
				   (i == 2) ? B[i].buf : dummy,
				   B[i].mask,
				   w.cols, &B[i].colors, B[i].type);
	}

	for (col = 0; col < w.cols; col++) {
	    if (B[0].mask && B[1].mask && B[2].mask) {
		putc(B[0].buf[col], fp);
		putc(B[1].buf[col], fp);
		putc(B[2].buf[col], fp);
	    }
	    else {
		putc(DEF_RED, fp);
		putc(DEF_GRN, fp);
		putc(DEF_BLU, fp);
	    }
	}
    }

    fclose(fp);

    for (i = 0; i < 3; i++) {
	G_free_colors(&B[i].colors);
	G_free(B[i].array);
	G_free(B[i].buf);
	G_free(B[i].mask);
	G_close_cell(B[i].file);
    }

    exit(EXIT_SUCCESS);
}
