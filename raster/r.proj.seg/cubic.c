/*
 * Name
 *  cubic.c -- use cubic convolution interpolation for given row, col
 *
 * Description
 *  cubic returns the value in the buffer that is the result of cubic
 *  convolution interpolation for the given row, column indices.
 *  If the given row or column is outside the bounds of the input map,
 *  the corresponding point in the output map is set to NULL.
 *
 *  If single surrounding points in the interpolation matrix are
 *  NULL they where filled with their neighbor
 */

#include <grass/gis.h>
#include <math.h>
#include "r.proj.h"


void p_cubic(struct cache *ibuffer,	/* input buffer                  */
	     void *obufptr,	/* ptr in output buffer          */
	     int cell_type,	/* raster map type of obufptr    */
	     double *col_idx,	/* column index (decimal)        */
	     double *row_idx,	/* row index (decimal)           */
	     struct Cell_head *cellhd	/* information of output map     */
    )
{
    int row;			/* row indices for interp        */
    int col;			/* column indices for interp     */
    int i, j;
    FCELL t, u;			/* intermediate slope            */
    FCELL result;		/* result of interpolation       */
      FCELL val[4];		/* buffer for temporary values   */
    FCELL cell[4][4];

    /* cut indices to integer */
    row = (int)floor(*row_idx - 0.5);
    col = (int)floor(*col_idx - 0.5);

    /* check for out of bounds of map - if out of bounds set NULL value     */
    if (row - 1 < 0 || row + 2 >= cellhd->rows ||
	col - 1 < 0 || col + 2 >= cellhd->cols) {
	G_set_null_value(obufptr, 1, cell_type);
	return;
    }

    for (i = 0; i < 4; i++)
	for (j = 0; j < 4; j++) {
	    const FCELL *cellp = CPTR(ibuffer, row - 1 + i, col - 1 + j);
	    if (G_is_f_null_value(cellp)) {
		G_set_null_value(obufptr, 1, cell_type);
		return;
	    }
	    cell[i][j] = *cellp;
	}

    /* do the interpolation  */
    t = *col_idx - 0.5 - col;
    u = *row_idx - 0.5 - row;

    for (i = 0; i < 4; i++) {
	const FCELL *tmp = cell[i];
	val[i] = G_interp_cubic(t, tmp[0], tmp[1], tmp[2], tmp[3]);
    }

    result = G_interp_cubic(u, val[0], val[1], val[2], val[3]);

    G_set_raster_value_f(obufptr, result, cell_type);
}
