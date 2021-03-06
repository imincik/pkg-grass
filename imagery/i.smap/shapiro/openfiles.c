#include <stdlib.h>
#include <grass/imagery.h>
#include <grass/glocale.h>
#include "bouman.h"
#include "local_proto.h"


int openfiles(struct parms *parms, struct files *files)
{
    struct Ref Ref;		/* subgroup reference list */
    int n;


    if (!I_get_subgroup_ref(parms->group, parms->subgroup, &Ref))
	G_fatal_error(_("Unable to read REF file for subgroup <%s> in group <%s>"),
		      parms->subgroup, parms->group);

    if (Ref.nfiles <= 0)
	G_fatal_error(_("Subgroup <%s> in group <%s> contains no raster maps"),
		      parms->subgroup, parms->group);

    /* allocate file descriptors, and io buffer */
    files->cellbuf = G_allocate_d_raster_buf();
    files->outbuf = G_allocate_c_raster_buf();
    files->isdata = G_malloc(G_window_cols());

    files->nbands = Ref.nfiles;
    files->band_fd = (int *)G_calloc(Ref.nfiles, sizeof(int));

    /* open all group maps for reading */
    for (n = 0; n < Ref.nfiles; n++)
	files->band_fd[n] =
	    open_cell_old(Ref.file[n].name, Ref.file[n].mapset);

    /* open output map */
    files->output_fd = open_cell_new(parms->output_map);

    return 0;
}
