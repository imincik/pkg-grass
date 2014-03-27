#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include "global.h"

void getcells(void)
{
    int fd, i, j;
    RASTER_MAP_TYPE data_type;
    CELL *ccell = NULL;
    FCELL *fcell = NULL;
    struct Cell_head inhead;
    char buf_wrns[32], buf_wrew[32], buf_mrns[32], buf_mrew[32];

    if ((fd = G_open_cell_old(iname, mapset)) < 0)
	G_fatal_error(_("Cannot open raster map <%s>"), iname);

    data_type = G_get_raster_map_type(fd);
    if (G_get_cellhd(iname, mapset, &inhead) < 0)
	G_fatal_error(_("Unable to read header of raster map <%s>"), iname);

    if (data_type == CELL_TYPE)
	ccell = (CELL *) G_malloc(sizeof(CELL) * window.cols);
    else if (data_type == FCELL_TYPE)
	fcell = (FCELL *) G_malloc(sizeof(FCELL) * window.cols);

    cell = (DCELL **) G_malloc(sizeof(DCELL *) * window.rows);
    atb = (DCELL **) G_malloc(sizeof(DCELL *) * window.rows);
    a = (DCELL **) G_malloc(sizeof(DCELL *) * window.rows);

    if (window.ew_res < inhead.ew_res || window.ns_res < inhead.ns_res) {
	G_format_resolution(window.ew_res, buf_wrew, G_projection());
	G_format_resolution(window.ns_res, buf_wrns, G_projection());
	G_format_resolution(inhead.ew_res, buf_mrew, G_projection());
	G_format_resolution(inhead.ns_res, buf_mrns, G_projection());
	G_fatal_error(_("The current region resolution [%s x %s] is finer "
			"than the input map's resolution [%s x %s]. "
			"The current region resolution must be identical "
			"to, or coarser than, the input map's resolution."),
		      buf_wrew, buf_wrns, buf_mrew, buf_mrns);
    }

    G_message(_("Reading elevation map..."));

    for (i = 0; i < window.rows; i++) {
	G_percent(i, window.rows, 2);

	cell[i] = (DCELL *) G_malloc(sizeof(DCELL) * window.cols);
	atb[i] = (DCELL *) G_malloc(sizeof(DCELL) * window.cols);
	a[i] = (DCELL *) G_malloc(sizeof(DCELL) * window.cols);

	if (data_type == CELL_TYPE) {
	    if (G_get_c_raster_row(fd, ccell, i) < 0) {
		G_close_cell(fd);
	    }
	    for (j = 0; j < window.cols; j++) {
		if (G_is_c_null_value(&ccell[j]))
		    G_set_d_null_value(&cell[i][j], 1);
		else
		    cell[i][j] = (DCELL) ccell[j];
	    }
	}
	else if (data_type == FCELL_TYPE) {
	    if (G_get_f_raster_row(fd, fcell, i) < 0) {
		G_close_cell(fd);
	    }
	    for (j = 0; j < window.cols; j++) {
		if (G_is_f_null_value(&fcell[j]))
		    G_set_d_null_value(&cell[i][j], 1);
		else
		    cell[i][j] = (DCELL) fcell[j];
	    }
	}
	else if (G_get_d_raster_row(fd, cell[i], i) < 0) {
	    G_close_cell(fd);
	    G_fatal_error(_("Unable to read raster map <%s> row %d"), iname,
			  i);
	}
    }
    if (data_type == CELL_TYPE)
	G_free(ccell);
    else if (data_type == FCELL_TYPE)
	G_free(fcell);
    G_percent(i, window.rows, 2);
    G_close_cell(fd);
}


void putcells(void)
{
    int fd, i;
    struct History history;

    if ((fd = G_open_raster_new(oname, DCELL_TYPE)) < 0)
	G_fatal_error(_("Cannot create raster map <%s>"), oname);

    G_message(_("Writing topographic index map..."));

    for (i = 0; i < window.rows; i++) {
	G_percent(i, window.rows, 2);
	G_put_d_raster_row(fd, atb[i]);
    }
    G_percent(i, window.rows, 2);
    G_close_cell(fd);

    G_short_history(oname, "raster", &history);
    strncpy(history.datsrc_1, iname, RECORD_LEN);
    history.datsrc_1[RECORD_LEN - 1] = '\0';	/* strncpy() doesn't null terminate if maxfill */
    G_write_history(oname, &history);
}
