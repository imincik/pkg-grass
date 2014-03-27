
/****************************************************************************
 *
 * MODULE:       g.list
 *               
 * AUTHOR(S):    Michael Shapiro,
 *               U.S.Army Construction Engineering Research Laboratory
 *               
 * PURPOSE:      Lists available GRASS data base files of the
 *               user-specified data type to standard output
 *
 * COPYRIGHT:    (C) 1999-2007 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#define MAIN
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grass/spawn.h>
#include "local_proto.h"
#include "list.h"

struct Option *element;

int main(int argc, char *argv[])
{
    int i, n, len;
    struct GModule *module;
    struct Option *mapset;
    struct Flag *full;
    char *str;

    init(argv[0]);

    module = G_define_module();
    module->keywords = _("general, map management");
    module->description =
	_("Lists available GRASS data base files "
	  "of the user-specified data type.");
    
    element = G_define_option();
    element->key = "type";
    element->key_desc = "datatype";
    element->type = TYPE_STRING;
    element->required = YES;
    element->multiple = YES;
    element->description = "Data type";
    for (len = 0, n = 0; n < nlist; n++)
	len += strlen(list[n].alias) + 1;
    str = G_malloc(len);

    for (n = 0; n < nlist; n++) {
	if (n) {
	    G_strcat(str, ",");
	    G_strcat(str, list[n].alias);
	}
	else
	    G_strcpy(str, list[n].alias);
    }
    element->options = str;

    mapset = G_define_option();
    mapset->key = "mapset";
    mapset->type = TYPE_STRING;
    mapset->required = NO;
    mapset->multiple = NO;
    mapset->description = _("Mapset to list (default: current search path)");

    full = G_define_flag();
    full->key = 'f';
    full->description = _("Verbose listing (also list map titles)");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    if (mapset->answer == NULL)
	mapset->answer = "";

    if (G_strcasecmp(mapset->answer, ".") == 0)
	mapset->answer = G_mapset();

    i = 0;
    while (element->answers[i]) {
	n = parse(element->answers[i]);

	if (full->answer) {
	    char lister[GPATH_MAX];

	    sprintf(lister, "%s/etc/lister/%s", G_gisbase(),
		    list[n].element[0]);
	    G_debug(3, "lister CMD: %s", lister);
	    if (access(lister, 1) == 0)	/* execute permission? */
		G_spawn(lister, lister, mapset->answer, NULL);
	    else
		do_list(n, mapset->answer);
	}
	else {
	    do_list(n, mapset->answer);
	}

	i++;
    }

    exit(EXIT_SUCCESS);
}

int parse(const char *data_type)
{
    int n;

    for (n = 0; n < nlist; n++) {
	if (G_strcasecmp(list[n].alias, data_type) == 0)
	    break;
    }

    return n;
}
