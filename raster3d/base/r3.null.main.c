
/***************************************************************************
* MODULE:       r3.null
*
* AUTHOR(S):    Roman Waupotitsch, Michael Shapiro, Helena Mitasova,
*               Bill Brown, Lubos Mitas, Jaro Hofierka
*
* PURPOSE:      Explicitly create the 3D NULL-value bitmap file.
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
#include "mask_functions.h"
#include <grass/glocale.h>


#define MAX(a,b) (a > b ? a : b)


typedef struct
{
    struct Option *map, *setNull, *null;
} paramType;

static paramType params;


/* function prototypes */
static void setParams(void);
static void getParams(char **name, d_Mask ** maskRules, int *changeNull,
		      double *newNullVal);
static void modifyNull(char *name, d_Mask * maskRules, int changeNull,
		       double newNullVal);

extern void *G3d_openNewParam();


static void setParams(void)
{
    params.map = G_define_option();
    params.map->key = "map";
    params.map->type = TYPE_STRING;
    params.map->required = YES;
    params.map->multiple = NO;
    params.map->gisprompt = "old,grid3,3d-raster";
    params.map->description =
	_("3d raster map for which to modify null values");

    params.setNull = G_define_option();
    params.setNull->key = "setnull";
    params.setNull->key_desc = "val[-val]";
    params.setNull->type = TYPE_STRING;
    params.setNull->required = NO;
    params.setNull->multiple = YES;
    params.setNull->description = _("List of cell values to be set to NULL");

    params.null = G_define_option();
    params.null->key = "null";
    params.null->type = TYPE_DOUBLE;
    params.null->required = NO;
    params.null->multiple = NO;
    params.null->description = _("The value to replace the null value by");
}

/*--------------------------------------------------------------------------*/

static void
getParams(char **name, d_Mask ** maskRules, int *changeNull,
	  double *newNullVal)
{
    *name = params.map->answer;
    parse_vallist(params.setNull->answers, maskRules);

    *changeNull = (params.null->answer != NULL);
    if (*changeNull)
	if (sscanf(params.null->answer, "%lf", newNullVal) != 1)
	    G3d_fatalError(_("Illegal value for null"));
}

/*-------------------------------------------------------------------------*/

static void
modifyNull(char *name, d_Mask * maskRules, int changeNull, double newNullVal)
{
    void *map, *mapOut;
    G3D_Region region;
    int tileX, tileY, tileZ, x, y, z;
    double value;
    int doCompress, doLzw, doRle, precision;
    int cacheSize;

    cacheSize = G3d_cacheSizeEncode(G3D_USE_CACHE_XY, 1);

    if (NULL == G_find_grid3(name, ""))
	G3d_fatalError(_("3D raster map <%s> not found"), name);

    fprintf(stderr, "name %s Mapset %s \n", name, G_mapset());
    map = G3d_openCellOld(name, G_mapset(), G3D_DEFAULT_WINDOW,
			  DCELL_TYPE, cacheSize);

    if (map == NULL)
	G3d_fatalError(_("Unable to open 3D raster map <%s>"), name);

    G3d_getRegionStructMap(map, &region);
    G3d_getTileDimensionsMap(map, &tileX, &tileY, &tileZ);

    G3d_getCompressionMode(&doCompress, &doLzw, &doRle, &precision);

    mapOut = G3d_openNewParam(name, DCELL_TYPE, G3D_USE_CACHE_XY,
			      &region, G3d_fileTypeMap(map),
			      doLzw, doRle, G3d_tilePrecisionMap(map), tileX,
			      tileY, tileZ);
    if (mapOut == NULL)
	G3d_fatalError(_("modifyNull: error opening tmp file"));

    G3d_minUnlocked(map, G3D_USE_CACHE_X);
    G3d_autolockOn(map);
    G3d_unlockAll(map);
    G3d_minUnlocked(mapOut, G3D_USE_CACHE_X);
    G3d_autolockOn(mapOut);
    G3d_unlockAll(mapOut);

     /*AV*/
	/* BEGIN OF ORIGINAL CODE */
	/*
	 * for (z = 0; z < region.depths; z++) {
	 * if ((z % tileZ) == 0) {
	 * G3d_unlockAll (map);
	 * G3d_unlockAll (mapOut);
	 * }
	 * for (y = 0; y < region.cols; y++)
	 * for (x = 0; x < region.rows; x++) {
	 */
	/* END OF ORIGINAL CODE */
	 /*AV*/
	/* BEGIN OF MY CODE */
	for (z = 0; z < region.depths; z++) {
	if ((z % tileZ) == 0) {
	    G3d_unlockAll(map);
	    G3d_unlockAll(mapOut);
	}
	for (y = region.rows - 1; y >= 0; y--)
	    for (x = 0; x < region.cols; x++) {
		/* END OF MY CODE */

		value = G3d_getDoubleRegion(map, x, y, z);

		if (G3d_isNullValueNum(&value, DCELL_TYPE)) {
		    if (changeNull) {
			value = newNullVal;
		    }
		}
		else if (mask_d_select((DCELL *) & value, maskRules)) {
		    G3d_setNullValue(&value, 1, DCELL_TYPE);
		}

		G3d_putDouble(mapOut, x, y, z, value);
	    }
	if ((z % tileZ) == 0) {
	    if (!G3d_flushTilesInCube
		(mapOut, 0, 0, MAX(0, z - tileZ), region.rows - 1,
		 region.cols - 1, z))
		G3d_fatalError(_("modifyNull: error flushing tiles in cube"));
	}
    }


    if (!G3d_flushAllTiles(mapOut))
	G3d_fatalError(_("modifyNull: error flushing all tiles"));

    G3d_autolockOff(map);
    G3d_unlockAll(map);
    G3d_autolockOff(mapOut);
    G3d_unlockAll(mapOut);

    if (!G3d_closeCell(map))
	G3d_fatalError(_("Unable to close raster map"));
    if (!G3d_closeCell(mapOut))
	G3d_fatalError(_("modifyNull: Unable to close tmp file"));
}

/*--------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    char *name;
    d_Mask *maskRules;
    int changeNull;
    double newNullVal;
    struct GModule *module;

    G_gisinit(argv[0]);
    module = G_define_module();
    module->keywords = _("raster3d, voxel");
    module->description =
	_("Explicitly create the 3D NULL-value bitmap file.");

    setParams();
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);
    getParams(&name, &maskRules, &changeNull, &newNullVal);

    modifyNull(name, maskRules, changeNull, newNullVal);

    exit(EXIT_SUCCESS);
}
