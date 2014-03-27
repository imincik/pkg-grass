
/****************************************************************************
 *
 * MODULE:       r.describe
 *
 * AUTHOR(S):    Michael Shapiro - CERL
 *
 * PURPOSE:      Prints terse list of category values found in a raster 
 *               map layer.
 *
 * COPYRIGHT:    (C) 2006 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <grass/gis.h>
#include "local_proto.h"
#include <grass/glocale.h>


int main(int argc, char *argv[])
{
    int as_int;
    int compact;
    int range;
    int windowed;
    int nsteps;
    char name[GNAME_MAX];
    char *mapset;
    char *no_data_str;
    struct GModule *module;
    struct
    {
	struct Flag *one;
	struct Flag *r;
	struct Flag *d;
	struct Flag *i;
	struct Flag *n;
    } flag;
    struct
    {
	struct Option *map;
	struct Option *nv;
	struct Option *nsteps;
    } option;

    /* please, remove before GRASS 7 released */
    struct Flag *q_flag;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, metadata");
    module->description =
	_("Prints terse list of category values found in a raster map layer.");

    /* define different options */
    option.map = G_define_standard_option(G_OPT_R_MAP);

    option.nv = G_define_option();
    option.nv->key = "nv";
    option.nv->type = TYPE_STRING;
    option.nv->required = NO;
    option.nv->multiple = NO;
    option.nv->answer = "*";
    option.nv->description = _("String representing no data cell value");

    option.nsteps = G_define_option();
    option.nsteps->key = "nsteps";
    option.nsteps->type = TYPE_INTEGER;
    option.nsteps->required = NO;
    option.nsteps->multiple = NO;
    option.nsteps->answer = "255";
    option.nsteps->description = _("Number of quantization steps");

    /*define the different flags */

    flag.one = G_define_flag();
    flag.one->key = '1';
    flag.one->description = _("Print the output one value per line");

    flag.r = G_define_flag();
    flag.r->key = 'r';
    flag.r->description = _("Only print the range of the data");

    flag.n = G_define_flag();
    flag.n->key = 'n';
    flag.n->description = _("Suppress reporting of any NULLs");

    flag.d = G_define_flag();
    flag.d->key = 'd';
    flag.d->description = _("Use the current region");

    flag.i = G_define_flag();
    flag.i->key = 'i';
    flag.i->description = _("Read fp map as integer");

    /* please, remove before GRASS 7 released */
    q_flag = G_define_flag();
    q_flag->key = 'q';
    q_flag->description = _("Run quietly");

    if (0 > G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* please, remove before GRASS 7 released */
    if (q_flag->answer) {
	G_putenv("GRASS_VERBOSE", "0");
	G_warning(_("The '-q' flag is superseded and will be removed "
		    "in future. Please use '--quiet' instead."));
    }

    compact = (!flag.one->answer);
    range = flag.r->answer;
    windowed = flag.d->answer;
    as_int = flag.i->answer;
    no_data_str = option.nv->answer;

    if (sscanf(option.nsteps->answer, "%d", &nsteps) != 1 || nsteps < 1)
	G_fatal_error(_("%s = %s -- must be greater than zero"),
		      option.nsteps->key, option.nsteps->answer);

    strcpy(name, option.map->answer);

    if ((mapset = G_find_cell2(name, ""))) {
	describe(name, mapset, compact, no_data_str,
		 range, windowed, nsteps, as_int, flag.n->answer);
	exit(EXIT_SUCCESS);
    }

    G_fatal_error(_("%s: [%s] not found"), G_program_name(), name);

    return EXIT_SUCCESS;
}
