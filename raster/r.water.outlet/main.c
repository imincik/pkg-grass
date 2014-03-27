
/****************************************************************************
 *
 * MODULE:       r.water.outlet
 * AUTHOR(S):    Charles Ehlschlaeger, USACERL (original contributor)
 *               Markus Neteler <neteler itc.it>, 
 *               Roberto Flor <flor itc.it>, 
 *               Bernhard Reiter <bernhard intevation.de>, 
 *               Huidae Cho <grass4u gmail.com>, 
 *               Glynn Clements <glynn gclements.plus.com>, 
 *               Jan-Oliver Wagner <jan intevation.de>, 
 *               Soeren Gebbert <soeren.gebbert gmx.de>
 * PURPOSE:      this program makes a watershed basin raster map using the 
 *               drainage pointer map, from an outlet point defined by an 
 *               easting and a northing.
 * COPYRIGHT:    (C) 1999-2006 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define MAIN
#include "basin.h"
#include "outletP.h"
#undef MAIN
#include <grass/gis.h>
#include <grass/glocale.h>

int main(int argc, char *argv[])
{
    double N, E;
    int row, col, basin_fd, drain_fd;
    CELL *cell_buf;
    char drain_name[GNAME_MAX], *drain_mapset, E_f, dr_f, ba_f, N_f, errr;
    struct GModule *module;
    struct Option *opt1, *opt2, *opt3, *opt4;
    char *buf;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, hydrology");
    module->description = _("Watershed basin creation program.");
    
    opt1 = G_define_option();
    opt1->key = "drainage";
    opt1->type = TYPE_STRING;
    opt1->required = YES;
    opt1->gisprompt = "old,cell,raster";
    opt1->description = _("Name of input raster map");

    opt2 = G_define_option();
    opt2->key = "basin";
    opt2->type = TYPE_STRING;
    opt2->required = YES;
    opt2->gisprompt = "new,cell,raster";
    opt2->description = _("Name of raster map to contain results");

    opt3 = G_define_option();
    opt3->key = "easting";
    opt3->type = TYPE_STRING;
    opt3->key_desc = "x";
    opt3->multiple = NO;
    opt3->required = YES;
    opt3->description = _("The map E grid coordinates");

    opt4 = G_define_option();
    opt4->key = "northing";
    opt4->type = TYPE_STRING;
    opt4->key_desc = "y";
    opt4->multiple = NO;
    opt4->required = YES;
    opt4->description = _("The map N grid coordinates");

    /*   Parse command line */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    if (G_get_window(&window) < 0) {
	G_asprintf(&buf, _("Unable to read current window parameters"));
	G_fatal_error(buf);
    }

    strcpy(drain_name, opt1->answer);
    strcpy(basin_name, opt2->answer);
    if (!G_scan_easting(*opt3->answers, &E, G_projection())) {
	G_warning(_("Illegal east coordinate <%s>\n"), opt3->answer);
	G_usage();
	exit(EXIT_FAILURE);
    }
    if (!G_scan_northing(*opt4->answers, &N, G_projection())) {
	G_warning(_("Illegal north coordinate <%s>\n"), opt4->answer);
	G_usage();
	exit(EXIT_FAILURE);
    }

    if (E < window.west || E > window.east || N < window.south ||
	N > window.north) {
	G_warning(_("Warning, ignoring point outside window: \n    %.4f,%.4f\n"),
		  E, N);
    }

    G_get_set_window(&window);
    dr_f = ba_f = N_f = E_f = errr = 0;

    drain_mapset = do_exist(drain_name);
    do_legal(basin_name);
    nrows = G_window_rows();
    ncols = G_window_cols();
    total = nrows * ncols;
    nrows_less_one = nrows - 1;
    ncols_less_one = ncols - 1;
    drain_fd = G_open_cell_old(drain_name, drain_mapset);

    if (drain_fd < 0)
	G_fatal_error(_("Unable to open drainage pointer map"));

    drain_ptrs =
	(char *)G_malloc(sizeof(char) * size_array(&pt_seg, nrows, ncols));
    bas = (CELL *) G_calloc(size_array(&ba_seg, nrows, ncols), sizeof(CELL));
    cell_buf = G_allocate_cell_buf();

    for (row = 0; row < nrows; row++) {
	G_get_map_row(drain_fd, cell_buf, row);
	for (col = 0; col < ncols; col++) {
	    if (cell_buf[col] == 0) 
		total--;
	    drain_ptrs[SEG_INDEX(pt_seg, row, col)] = cell_buf[col];
	}
    }
    G_free(cell_buf);
    row = (window.north - N) / window.ns_res;
    col = (E - window.west) / window.ew_res;
    if (row >= 0 && col >= 0 && row < nrows && col < ncols)
	overland_cells(row, col);
    G_free(drain_ptrs);
    cell_buf = G_allocate_cell_buf();
    basin_fd = G_open_cell_new(basin_name);

    if (basin_fd < 0)
	G_fatal_error(_("Unable to open new basin map"));

    for (row = 0; row < nrows; row++) {
	for (col = 0; col < ncols; col++) {
	    cell_buf[col] = bas[SEG_INDEX(ba_seg, row, col)];
	    if (cell_buf[col] == 0)
		G_set_null_value(&cell_buf[col], 1, CELL_TYPE);
	}
	G_put_raster_row(basin_fd, cell_buf, CELL_TYPE);
    }
    G_free(bas);
    G_free(cell_buf);
    if (G_close_cell(basin_fd) < 0)
	G_fatal_error(_("Unable to close new basin map layer"));

    return 0;
}
