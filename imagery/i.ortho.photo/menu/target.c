#include <unistd.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/imagery.h>
#include <grass/glocale.h>
#include "local_proto.h"

int get_target(char *name, struct Cell_head *target_window)
{
    char location[40];
    char mapset[40];
    char buf[1024];
    int stat;

    if (!I_get_target(name, location, mapset)) {
	sprintf(buf, "Target information for group [%s] missing.\n", name);
	goto error;
    }

    sprintf(buf, "%s/%s", G_gisdbase(), location);
    if (access(buf, 0) != 0) {
	sprintf(buf, "Target location [%s] not found\n", location);
	goto error;
    }
    select_target_env();
    G__setenv("LOCATION_NAME", location);
    stat = G__mapset_permissions(mapset);
    if (stat > 0) {
	G__setenv("MAPSET", mapset);
	G_get_window(target_window);
	select_current_env();
	return 1;
    }
    sprintf(buf, "Mapset [%s] in target location [%s] - ", mapset, location);
    strcat(buf, stat == 0 ? "permission denied\n" : "not found\n");
  error:
    G_fatal_error(buf);
}
