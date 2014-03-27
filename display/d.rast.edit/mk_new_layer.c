/*

   Chris Rewerts, Agricultural Engineering, Purdue University
   April 1991

   This function copies the edited data from the temporary
   file to the new cell layer. Also installs colr, cats,
   and hist support files.

 */

#include "edit.h"

int make_new_cell_layer(void)
{
    struct History hist;
    void *rast;
    int cellfd;

    int tmpfd;
    int row;

    /* open the new raster map to contain the edited version of
       the original cell layer. open our temporary file for read
       and copy its contents to the layer */

    G_set_window(&real_window);

    cellfd = G_open_raster_new(new_name, map_type);
    tmpfd = open(tempfile, 0);
    lseek(tmpfd, 0L, 0);

    rast = G_allocate_raster_buf(map_type);

    fprintf(stderr, "\n     +-------------------------------------------+\n");
    fprintf(stderr, "     |         Saving new cell layer             |\n");
    fprintf(stderr, "     +---------------------------------------");


    for (row = 0; row < real_nrows; row++) {
	if (read(tmpfd, rast, real_ncols * cellsize) !=
	    (real_ncols * cellsize))
	    error(1, "error writing raster map during copy");
	G_put_raster_row(cellfd, rast, map_type);
	G_percent(row, real_nrows, 5);
    }
    G_percent(100, 100, 5);
    fprintf(stderr, "\n");

    close(tmpfd);
    G_close_cell(cellfd);
    unlink(tempfile);

    /* create and write cat, colr, quant, and hist support files
       for the newly created layer */

    if (colr_ok) {
	G_write_colors(new_name, user_mapset, &colr);
	G_free_colors(&colr);
	colr_ok = 0;
    }
    if (cats_ok) {
	cats.num = G_number_of_cats(new_name, user_mapset);
	G_write_cats(new_name, &cats);
	G_free_cats(&cats);
	cats_ok = 0;
    }
    if (quant_ok) {
	G_write_quant(new_name, G_mapset(), &quant);
	G_quant_free(&quant);
	cats_ok = 0;
    }

    /* construct some history information */
    sprintf(hist.mapid, "%s", G_date());
    sprintf(hist.title, "%s", new_name);
    sprintf(hist.mapset, "%s", user_mapset);
    sprintf(hist.creator, "%s", G_whoami());
    sprintf(hist.maptype, "cell");
    sprintf(hist.edhist[0],
	    "Generated by d.rast.edit from original raster map");
    sprintf(hist.edhist[1], "  %s in mapset %s ", orig_name, orig_mapset);
    hist.edlinecnt = 2;

    /* write history */
    if (G_write_history(new_name, &hist) == -1)
	error(0, "could not write history");

    return 0;
}