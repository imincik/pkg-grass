#include <stdlib.h>
#include <grass/imagery.h>
#include <grass/glocale.h>
#include "signature.h"
#include "files.h"
#include "local_proto.h"


int compute_means(struct files *files, struct Signature *S)
{
    int n, n_nulls;
    int b;
    int nrows, ncols, row, col;
    CELL *class, *cell;

    for (n = 0; n < S->nsigs; n++)	/* for each signature (aka class) */
	for (b = 0; b < S->nbands; b++)	/* for each band file */
	    S->sig[n].mean[b] = 0.0;

    nrows = G_window_rows();
    ncols = G_window_cols();
    class = (CELL *) G_calloc(ncols, sizeof(CELL));

    G_message(_("Calculating class means..."));

    for (row = 0; row < nrows; row++) {
	G_percent(row, nrows, 2);
	read_training_map(class, row, ncols, files);
	for (b = 0; b < files->nbands; b++) {	/* NOTE: files->nbands == S->nbands */
	    if (G_get_c_raster_row
		(files->band_fd[b], cell = files->band_cell[b], row) < 0)
		exit(1);
	    for (col = 0; col < ncols; col++) {
		if (G_is_c_null_value(&cell[col])) {
		    n_nulls++;
		    continue;
		}
		n = class[col];
		if (n < 0)
		    continue;
		S->sig[n].mean[b] += cell[col];
	    }
	}
    }
    G_percent(nrows, nrows, 2);

    for (n = 0; n < S->nsigs; n++)	/* for each signature (aka class) */
	for (b = 0; b < S->nbands; b++)	/* for each band file */
	    S->sig[n].mean[b] /= S->sig[n].npoints;
    G_free(class);

    return 0;
}
