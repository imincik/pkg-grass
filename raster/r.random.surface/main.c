
/****************************************************************************
 *
 * MODULE:       r.random.surface
 * AUTHOR(S):    Charles Ehlschlaeger, Michael Goodchild, and Chih-chang Lin; 
 *                      (National Center for Geographic Information and 
 *                      Analysis, University of California, Santa Barbara)
 *                      (original contributors)
 *               Markus Neteler <neteler itc.it>, 
 *               Bernhard Reiter <bernhard intevation.de>, 
 *               Brad Douglas <rez touchofmadness.com>, 
 *               Glynn Clements <glynn gclements.plus.com>, 
 *               Jachym Cepicky <jachym les-ejk.cz>, 
 *               Jan-Oliver Wagner <jan intevation.de>
 * PURPOSE:      generates a spatially dependent random surface
 * COPYRIGHT:    (C) 2000-2008 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/
/* main.c */
#include <stdlib.h>
#include <stdio.h>
#include <grass/gis.h>
#include <grass/glocale.h>

#define MAIN
#include "ransurf.h"
#include "local_proto.h"
#undef MAIN

int main(int argc, char **argv)
{
    struct GModule *module;
    int DoMap, DoFilter, MapSeed;
    double ran1();

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, random, surface");
    module->description =
	_("Generates random surface(s) with spatial dependence.");

    Init(argc, argv);
    if (Uniform->answer)
	GenNorm();
    CalcSD();
    for (DoMap = 0; DoMap < NumMaps; DoMap++) {
	OutFD = G_open_cell_new(OutNames[DoMap]);
	if (OutFD < 0)
	    G_fatal_error(_("Unable to open raster map <%s>"),
			  OutNames[DoMap]);

	G_message(_("Generating raster map <%s>..."),
		  OutNames[DoMap]);

	if (Seeds[DoMap] == SEED_MIN - 1)
	    Seeds[DoMap] = (int)(ran1() * SEED_MAX);

	MapSeed = Seed = Seeds[DoMap];
	ZeroMapCells();

	for (DoFilter = 0; DoFilter < NumFilters; DoFilter++) {
	    CopyFilter(&Filter, AllFilters[DoFilter]);
	    G_debug(1, "Starting filter #%d, distance: %.*lf, exponent: %.*lf, flat: %.*lf",
		    DoFilter, Digits(2.0 * Filter.MaxDist, 6),
		      2.0 * Filter.MaxDist, Digits(1.0 / Filter.Exp, 6),
		      1.0 / Filter.Exp, Digits(Filter.Mult, 6), Filter.Mult);

	    MakeBigF();
	    CalcSurface();
	}

	SaveMap(DoMap, MapSeed);
    }

    G_done_msg(" ");
    
    exit(EXIT_SUCCESS);
}
