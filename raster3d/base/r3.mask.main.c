
/***************************************************************************
* MODULE:       r3.mask
*
* AUTHOR(S):    Roman Waupotitsch, Michael Shapiro, Helena Mitasova,
*		Bill Brown, Lubos Mitas, Jaro Hofierka
*
* PURPOSE:      Establishes the current working 3D raster mask.
*
* COPYRIGHT:    (C) 2005 by the GRASS Development Team
*
*               This program is free software under the GNU General Public
*               License (>=v2). Read the file COPYING that comes with GRASS
*               for details.
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/G3d.h>
#include <grass/glocale.h>
#include "mask_functions.h"

/*--------------------------------------------------------------------------*/

typedef struct
{
    struct Option *map, *maskVals;
} paramType;

static paramType params;
extern void *G3d_openNewParam();

static void setParams()
{
    params.map = G_define_option();
    params.map->key = "map";
    params.map->type = TYPE_STRING;
    params.map->required = YES;
    params.map->multiple = NO;
    params.map->gisprompt = "old,grid3,3d-raster";
    params.map->description = _("3D raster map with reference values");

    params.maskVals = G_define_option();
    params.maskVals->key = "maskvalues";
    params.maskVals->key_desc = "val[-val]";
    params.maskVals->type = TYPE_STRING;
    params.maskVals->required = NO;
    params.maskVals->multiple = YES;
    params.maskVals->description = _("List of cell values to be masked out");
}

/*--------------------------------------------------------------------------*/

void getParams(char **name, d_Mask ** maskRules)
{
    *name = params.map->answer;
    parse_vallist(params.maskVals->answers, maskRules);
}

/*-------------------------------------------------------------------------*/

#define MAX(a,b) (a > b ? a : b)

static void makeMask(char *name, d_Mask * maskRules)
{
    void *map, *mask;
    G3D_Region region;
    int tileX, tileY, tileZ, x, y, z, cacheSize;
    double value;
    float floatNull;

    cacheSize = G3d_cacheSizeEncode(G3D_USE_CACHE_XY, 1);

    if (NULL == G_find_grid3(name, ""))
	G3d_fatalError(_("3D raster map <%s> not found"), name);

    map = G3d_openCellOld(name, G_mapset(), G3D_DEFAULT_WINDOW,
			  DCELL_TYPE, cacheSize);

    if (map == NULL)
	G3d_fatalError(_("Unable to open 3D raster map <%s>"), name);

    G3d_getRegionStructMap(map, &region);

    G3d_getTileDimensionsMap(map, &tileX, &tileY, &tileZ);

    mask = G3d_openNewParam(G3d_maskFile(), FCELL_TYPE, cacheSize,
			    &region, FCELL_TYPE, G3D_NO_LZW, G3D_USE_RLE, 0,
			    tileX, tileY, tileZ);

    if (mask == NULL)
	G3d_fatalError(_("Unable to open 3D raster mask file"));

    G3d_minUnlocked(map, G3D_USE_CACHE_X);
    G3d_autolockOn(map);
    G3d_unlockAll(map);
    G3d_minUnlocked(mask, G3D_USE_CACHE_X);
    G3d_autolockOn(mask);
    G3d_unlockAll(mask);

    G3d_setNullValue(&floatNull, 1, FCELL_TYPE);

    for (z = 0; z < region.depths; z++) {
	if ((z % tileZ) == 0) {
	    G3d_unlockAll(map);
	    G3d_unlockAll(mask);
	}
	/*for (y = region.rows-1; y >= 0; y--) *//* go north to south */
	for (y = 0; y < region.rows; y++)	/*I dont know which one is right; soeren 04 Aug 05 */
	    for (x = 0; x < region.cols; x++) {
		value = G3d_getDoubleRegion(map, x, y, z);
		if (mask_d_select((DCELL *) & value, maskRules))
		    G3d_putFloat(mask, x, y, z, (float)floatNull);	/* mask-out value */
		else
		    G3d_putFloat(mask, x, y, z, (float)0.0);	/* not mask-out value */
	    }
	if ((z % tileZ) == 0) {
	    if (!G3d_flushTilesInCube
		(mask, 0, 0, MAX(0, z - tileZ), region.rows - 1,
		 region.cols - 1, z))
		G3d_fatalError(_("makeMask: error flushing tiles in cube"));
	}
    }

    if (!G3d_flushAllTiles(mask))
	G3d_fatalError(_("makeMask: error flushing all tiles"));

    G3d_autolockOff(map);
    G3d_unlockAll(map);
    G3d_autolockOff(mask);
    G3d_unlockAll(mask);

    if (!G3d_closeCell(mask))
	G3d_fatalError(_("Unable to close 3D raster mask file"));
    if (!G3d_closeCell(map))
	G3d_fatalError(_("Unable to close raster map <%s>"), name);
}

/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    char *name;
    d_Mask *maskRules;
    struct GModule *module;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster3d, voxel");
    module->description =
	_("Establishes the current working 3D raster mask.");


    if (G3d_maskFileExists())
	G_fatal_error(_("Cannot create mask file: G3D_MASK already exists!\n Use 'g.remove rast3d=G3D_MASK' to remove the existing mask."));

    setParams();
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);
    getParams(&name, &maskRules);

    makeMask(name, maskRules);

    exit(EXIT_SUCCESS);
}

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
