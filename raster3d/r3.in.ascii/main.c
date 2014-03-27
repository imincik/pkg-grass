
/***************************************************************************
*
* MODULE:       r3.in.ascii
*
* AUTHOR(S):    Roman Waupotitsch, Michael Shapiro, Helena Mitasova, Bill Brown, 
* 		Lubos Mitas, Jaro Hofierka 
*
* PURPOSE:      Convert a 3D ASCII raster text file into a (binary) 3D raster map layer 
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

/*- prototypes --------------------------------------------------------------*/

static void fatalError(char *errorMsg);	/*Simple Error message */

static void setParams();	/*Fill the paramType structure */

/*Puts the userdefined parameters into easier handable variables */
static void getParams(char **input, char **output, int *convertNull,
		      char *nullValu);

/*reads a g3d ascii-file headerfile-string */
static void readHeaderString(FILE * fp, char *valueString, double *value);

static FILE *openAscii(char *asciiFile, G3D_Region * region);	/*open the g3d ascii file */

/*This function does all the work, it reads the values from the g3d ascii-file and put 
   it into an g3d-map */
static void asciiToG3d(FILE * fp, G3D_Region * region, int convertNull,
		       char *nullValue);


/*---------------------------------------------------------------------------*/

void *map = NULL;
extern void *G3d_openNewParam();

/*---------------------------------------------------------------------------*/
static void fatalError(char *errorMsg)
{
    if (map != NULL) {
	/* should unopen map here! */
	G3d_closeCell(map);
    }

    G3d_fatalError(errorMsg);
}

/*---------------------------------------------------------------------------*/

typedef struct
{
    struct Option *input, *output, *nv;
} paramType;

static paramType param;

static void setParams()
{
    param.input = G_define_option();
    param.input->key = "input";
    param.input->type = TYPE_STRING;
    param.input->required = YES;
    param.input->key_desc = "name";
    param.input->gisprompt = "old_file,file,input";
    param.input->description = _("ASCII raster map to be imported");

    param.output = G_define_standard_option(G_OPT_R3_OUTPUT);

    param.nv = G_define_option();
    param.nv->key = "nv";
    param.nv->type = TYPE_STRING;
    param.nv->required = NO;
    param.nv->multiple = NO;
    param.nv->answer = "*";
    param.nv->description =
	_("String representing NULL value data cell (use 'none' if no such value)");
}

/*---------------------------------------------------------------------------*/

static void
getParams(char **input, char **output, int *convertNull, char *nullValue)
{
    *input = param.input->answer;
    *output = param.output->answer;
    *convertNull = (strcmp(param.nv->answer, "none") != 0);
    if (*convertNull)
	if (sscanf(param.nv->answer, "%s", nullValue) != 1)
	    fatalError("getParams: NULL-value value invalid");

    G_debug(3, "getParams: input: %s, output: %s", *input, *output);
}

/*---------------------------------------------------------------------------*/

static void readHeaderString(FILE * fp, char *valueString, double *value)
{
    static char format[100];

    /* to avoid buffer overflows we use snprintf */
    G_snprintf(format, 100, "%s %%lf", valueString);
    if (fscanf(fp, format, value) != 1) {
	G_debug(3, "bad value for [%s]", valueString);
	fatalError("readHeaderString: header value invalid");
    }
    while (fgetc(fp) != '\n') ;
}

/*---------------------------------------------------------------------------*/

static FILE *openAscii(char *asciiFile, G3D_Region * region)
{
    FILE *fp;
    double tmp;

    G_debug(3, "openAscii: opens the ascii file and reads the header");

    fp = fopen(asciiFile, "r");
    if (fp == NULL) {
	perror(asciiFile);
	G_usage();
	exit(EXIT_FAILURE);
    }

    G3d_getWindow(region);

    readHeaderString(fp, "north:", &(region->north));
    readHeaderString(fp, "south:", &(region->south));
    readHeaderString(fp, "east:", &(region->east));
    readHeaderString(fp, "west:", &(region->west));
    readHeaderString(fp, "top:", &(region->top));
    readHeaderString(fp, "bottom:", &(region->bottom));
    readHeaderString(fp, "rows:", &tmp);
    region->rows = tmp;
    readHeaderString(fp, "cols:", &tmp);
    region->cols = tmp;
    readHeaderString(fp, "levels:", &tmp);
    region->depths = tmp;

    return fp;
}

/*---------------------------------------------------------------------------*/

#define MAX(a,b) (a > b ? a : b)

static void
asciiToG3d(FILE * fp, G3D_Region * region, int convertNull, char *nullValue)
{
    int x, y, z;
    double value;
    char buff[256];
    int tileX, tileY, tileZ;


    G3d_getTileDimensionsMap(map, &tileX, &tileY, &tileZ);
    G3d_minUnlocked(map, G3D_USE_CACHE_X);

    G3d_autolockOn(map);
    G3d_unlockAll(map);
    G_message(_("Loading data ...  (%dx%dx%d)"), region->cols, region->rows,
	      region->depths);

    G_debug(3,
	    "asciiToG3d: writing the 3D raster map, with rows %i cols %i depths %i",
	    region->rows, region->cols, region->depths);

    for (z = 0; z < region->depths; z++) {

	if ((z % tileZ) == 0)
	    G3d_unlockAll(map);

	for (y = region->rows - 1; y >= 0; y--)	/* go south to north */
	    for (x = 0; x < region->cols; x++) {

                if (fscanf(fp, "%s", buff) != 1) {
                    if (feof(fp))
                        G_warning(_("End of file reached while still loading data."));
		    G_debug(3,
			    "missing data at col=%d row=%d depth=%d last_value=[%.4f]",
			    x + 1, region->rows - y, z + 1, value);
		    fatalError("asciiToG3d: read failed");
		}

                /* Check for null value */
                if (convertNull && strncmp(buff, nullValue, strlen(nullValue)) == 0) {
		    G3d_setNullValue(&value, 1, DCELL_TYPE);
		}
		else {
		    if (sscanf(buff, "%lf", &value) != 1) {
                        G_warning(_("Invalid value detected"));
                        G_debug(1, "invalid value at col=%d row=%d depth=%d last_value=[%s]",
				x + 1, region->rows - y, z + 1, buff);
			fatalError("asciiToG3d: read failed");
		    }
		}
		G3d_putDouble(map, x, y, z, value);
	    }

	if (!G3d_flushTilesInCube(map,
				  0, 0, MAX(0, z - tileZ),
				  region->rows - 1, region->cols - 1, z))
	    fatalError("asciiTog3d: error flushing tiles");

    }

    if (fscanf(fp, "%lf", &value) == 1) {
	G_warning(_("Data exists in input file after fully importing "
		    "expected data.  [%.4f ...]"), value);
    }

    if (!G3d_flushAllTiles(map))
	fatalError("asciiTog3d: error flushing tiles");

    G3d_autolockOff(map);
    G3d_unlockAll(map);

}

/*---------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    char *input, *output;
    int convertNull;
    char nullValue[256];
    int useTypeDefault, type, useLzwDefault, doLzw, useRleDefault, doRle;
    int usePrecisionDefault, precision, useDimensionDefault, tileX, tileY,
	tileZ;
    G3D_Region region;
    FILE *fp;
    struct GModule *module;
    struct History history;

    map = NULL;

    G_gisinit(argv[0]);
    module = G_define_module();
    module->keywords = _("raster3d, voxel, import");
    module->description =
	_("Converts a 3D ASCII raster text file into a (binary) 3D raster map layer.");

    setParams();
    G3d_setStandard3dInputParams();

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    getParams(&input, &output, &convertNull, nullValue);
    if (!G3d_getStandard3dParams(&useTypeDefault, &type,
				 &useLzwDefault, &doLzw,
				 &useRleDefault, &doRle,
				 &usePrecisionDefault, &precision,
				 &useDimensionDefault, &tileX, &tileY,
				 &tileZ))
	fatalError("Error getting standard parameters");

    fp = openAscii(input, &region);

    /*Open the new G3D map */
    map = G3d_openNewParam(output, DCELL_TYPE, G3D_USE_CACHE_XY,
			   &region,
			   type, doLzw, doRle, precision, tileX, tileY,
			   tileZ);

    if (map == NULL)
	fatalError(_("Unable to open 3D raster map"));

    /*Create the new G3D Map */
    asciiToG3d(fp, &region, convertNull, nullValue);

    if (!G3d_closeCell(map))
	fatalError(_("Unable to close 3D raster map"));

    /* write input name to map history */
    G3d_readHistory(output, G_mapset(), &history);
    strncpy(history.datsrc_1, input, RECORD_LEN);
    history.datsrc_1[RECORD_LEN - 1] = '\0';	/* strncpy() doesn't null terminate if maxfill */
    G3d_writeHistory(output, &history);

    map = NULL;
    if (fclose(fp))
	fatalError(_("Unable to close ASCII file"));

    G_done_msg(" ");

    return EXIT_SUCCESS;
}
