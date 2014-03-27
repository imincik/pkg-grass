
/**
 * \file myname.c
 *
 * \brief GIS Library - Database name functions.
 *
 * (C) 2001-2008 by the GRASS Development Team
 *
 * This program is free software under the GNU General Public License
 * (>=v2). Read the file COPYING that comes with GRASS for details.
 *
 * \author GRASS GIS Development Team
 *
 * \date 1999-2008
 */

#include <string.h>
#include <grass/gis.h>
#include <grass/glocale.h>


/**
 * \brief Returns location title.
 *
 * Returns a one line title for the database location. This title is 
 * read from the file MYNAME in the PERMANENT mapset. See also 
 * Permanent_Mapset for a discussion of the PERMANENT mapset.<br>
 *
 * <b>Note:</b> This name is the first line in the file 
 * $GISDBASE/$LOCATION_NAME/PERMANENT/MYNAME
 *
 * \return Pointer to a string
 */

char *G_myname(void)
{
    static char name[GNAME_MAX];
    char path[GPATH_MAX];
    FILE *fd;
    int ok;

    ok = 0;

    G__file_name(path, "", "MYNAME", "PERMANENT");
    if ((fd = fopen(path, "r"))) {
	ok = G_getl(name, sizeof name, fd);
	fclose(fd);
    }
    if (!ok)
	strcpy(name, _("This location has no description."));

    return name;
}
