
/****************************************************************************
*
* MODULE:       r.to.rast3elev 
*   	    	
* AUTHOR(S):    Original author 
*               Soeren Gebbert soerengebbert@gmx.de
* 		07 08 2006 Berlin
* PURPOSE:      Creates a 3D volume map based on 2D elevation and value raster maps
*
* COPYRIGHT:    (C) 2006 by the GRASS Development Team
*
*               This program is free software under the GNU General Public
*   	    	License (>=v2). Read the file COPYING that comes with GRASS
*   	    	for details.
*
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/G3d.h>
#include <grass/glocale.h>
#include <grass/config.h>

/*- params and global variables -----------------------------------------*/
typedef struct
{
    struct Option *input, *elev, *output, *upper, *lower;
    struct Flag *fillup, *filllow, *mask;
} paramType;

/*Data to be used */
typedef struct
{
    int mapnum;			/*The umber of input maps */
    int count;			/*3d raster map access counter */
    void *map;			/*The 3d voxel output map */
    int input;			/*The current raster value map pointer */
    int elev;			/*The current raster elevation map pointer */
    int inputmaptype;
    int elevmaptype;
    double upper;		/*The upper value */
    double lower;		/*The lower value */
    int useUpperVal;		/*0 = use upper value, 1 = use map value to fill upper cells */
    int useLowerVal;		/*0 = use lower value, 1 = use map value to fill lower cells */
} Database;

paramType param;		/*params */

/*- prototypes --------------------------------------------------------------*/
void fatal_error(Database db, char *errorMsg);	/*Simple Error message */
void set_params();		/*Fill the paramType structure */
void elev_raster_to_g3d(Database db, G3D_Region region);	/*Write the raster */
int open_input_raster_map(char *name, char *mapset);	/*opens the outputmap */
void close_input_raster_map(int fd);	/*close the map */
double get_raster_value_as_double(int maptype, void *ptr, double nullval);
void check_input_maps(Database * db);	/*Check input maps */


/* ************************************************************************* */
/* Get the value of the current raster pointer as double ******************* */
/* ************************************************************************* */
double get_raster_value_as_double(int MapType, void *ptr, double nullval)
{
    double val = nullval;

    if (MapType == CELL_TYPE) {
	if (G_is_null_value(ptr, MapType)) {
	    val = nullval;
	}
	else {
	    val = *(CELL *) ptr;
	}
    }
    if (MapType == FCELL_TYPE) {
	if (G_is_null_value(ptr, MapType)) {
	    val = nullval;
	}
	else {
	    val = *(FCELL *) ptr;
	}
    }
    if (MapType == DCELL_TYPE) {
	if (G_is_null_value(ptr, MapType)) {
	    val = nullval;
	}
	else {
	    val = *(DCELL *) ptr;
	}
    }

    return val;
}

/* ************************************************************************* */
/* Check the input maps **************************************************** */
/* ************************************************************************* */
void check_input_maps(Database * db)
{
    char *mapset = NULL;
    int i;
    int elevcount = 0, inputcount = 0;

    G_debug(2, "Checking input maps");

    /*Check elev maps */
    if (param.elev->answers != NULL) {
	for (i = 0; param.elev->answers[i] != NULL; i++) {

	    mapset = NULL;
	    mapset = G_find_cell2(param.elev->answers[i], "");
	    if (mapset == NULL) {
		G_fatal_error(_("Raster map <%s> not found"),
			      param.elev->answers[i]);
		exit(EXIT_FAILURE);
	    }
	    elevcount++;
	}
    }

    /*Check input maps */
    if (param.input->answers != NULL) {
	for (i = 0; param.input->answers[i] != NULL; i++) {

	    mapset = NULL;
	    mapset = G_find_cell2(param.input->answers[i], "");
	    if (mapset == NULL) {
		G_fatal_error(_("Raster map <%s> not found"),
			      param.input->answers[i]);
		exit(EXIT_FAILURE);
	    }
	    inputcount++;
	}
    }

    if (elevcount != inputcount) {
	G_fatal_error(_("The number of input and elevation maps is not equal"));
	exit(EXIT_FAILURE);
    }

    db->mapnum = inputcount;

    return;
}

/* ************************************************************************* */
/* Open the raster input map *********************************************** */
/* ************************************************************************* */
int open_input_raster_map(char *name, char *mapset)
{
    int fd;

    G_debug(3, "Open Raster file %s in Mapset %s", name, mapset);

    /* open raster map */
    fd = G_open_cell_old(name, mapset);

    if (fd < 0)
	G_fatal_error(_("Unable to open raster map <%s>"), name);

    return fd;
}

/* ************************************************************************* */
/* Close the raster input map ********************************************** */
/* ************************************************************************* */
void close_input_raster_map(int fd)
{
    if (G_close_cell(fd) < 0)
	G_fatal_error(_("Unable to close input map"));
}

/* ************************************************************************* */
/* Error handling ********************************************************** */
/* ************************************************************************* */
void fatal_error(Database db, char *errorMsg)
{
    /* Close files and exit */
    if (db.map != NULL) {
	/* should unopen map here! but this functionality is not jet implemented */
	if (!G3d_closeCell(db.map))
	    G3d_fatalError(_("Could not close the map"));
    }

    if (db.input)
	close_input_raster_map(db.input);

    if (db.elev)
	close_input_raster_map(db.elev);

    G3d_fatalError(errorMsg);
    exit(EXIT_FAILURE);
}

/* ************************************************************************* */
/* Set up the arguments **************************************************** */
/* ************************************************************************* */
void set_params()
{

    param.input = G_define_standard_option(G_OPT_R_INPUTS);
    param.elev = G_define_standard_option(G_OPT_R_ELEVS);
    param.output = G_define_standard_option(G_OPT_R3_OUTPUT);

    param.upper = G_define_option();
    param.upper->key = "upper";
    param.upper->type = TYPE_DOUBLE;
    param.upper->required = NO;
    param.upper->description =
	_("The value to fill the upper cells, default is null");

    param.lower = G_define_option();
    param.lower->key = "lower";
    param.lower->type = TYPE_DOUBLE;
    param.lower->required = NO;
    param.lower->description =
	_("The value to fill the lower cells, default is null");

    param.fillup = G_define_flag();
    param.fillup->key = 'u';
    param.fillup->description =
	_("Use the input map values to fill the upper cells");

    param.filllow = G_define_flag();
    param.filllow->key = 'l';
    param.filllow->description =
	_("Use the input map values to fill the lower cells");

    param.mask = G_define_flag();
    param.mask->key = 'm';
    param.mask->description = _("Use 3D raster mask (if exists) with input map");

    return;
}

/* ************************************************************************* */
/* Write the raster maps into the G3D map *********************************** */
/* ************************************************************************* */
void elev_raster_to_g3d(Database db, G3D_Region region)
{
    int x, y, z = 0;
    int rows, cols, depths;
    void *input_rast;
    void *input_ptr;
    void *elev_rast;
    void *elev_ptr;
    double inval, value, null;
    double height, top, bottom, tbres;

    rows = region.rows;
    cols = region.cols;
    depths = region.depths;
    top = region.top;
    bottom = region.bottom;

    /*Calculate the top-bottom resolution */
    tbres = (top - bottom) / depths;

    /*memory */
    input_rast = G_allocate_raster_buf(db.inputmaptype);
    elev_rast = G_allocate_raster_buf(db.elevmaptype);

    G3d_setNullValue(&null, 1, DCELL_TYPE);


    G_debug(3,
	    "elev_raster_to_g3d: Writing 3D raster map with depths %i rows %i cols %i and count %i.",
	    depths, rows, cols, db.count);

    /*The mainloop */
    for (y = 0; y < rows; y++) {
	G_percent(y, rows - 1, 10);

	if (!G_get_raster_row(db.input, input_rast, y, db.inputmaptype))
	    fatal_error(db, _("Could not get raster row from input map"));
	if (!G_get_raster_row(db.elev, elev_rast, y, db.elevmaptype))
	    fatal_error(db, _("Could not get raster row from elev map"));

	for (x = 0, input_ptr = input_rast, elev_ptr = elev_rast; x < cols;
	     x++, input_ptr =
	     G_incr_void_ptr(input_ptr, G_raster_size(db.inputmaptype)),
	     elev_ptr =
	     G_incr_void_ptr(elev_ptr, G_raster_size(db.elevmaptype))) {

	    /*Get the elevation and the input map value */
	    inval =
		get_raster_value_as_double(db.inputmaptype, input_ptr, null);
	    height =
		get_raster_value_as_double(db.elevmaptype, elev_ptr, null);

	    G_debug(4,
		    "Caluclating position in 3d region -> height %g with value %g",
		    height, inval);

	    /* Calculate if the G3D cell is lower or upper the elevation map
	     *  and set the value.*/
	    if (db.count == 0) {
		/*Use this method if the 3d raster map was not touched befor */
		for (z = 0; z < depths; z++) {

		    /*Upper cells */
		    if (height < (z * tbres + bottom)) {
			if (db.useUpperVal == 1)
			    value = inval;	/*Input map value */
			else
			    value = db.upper;
		    }
		    /*lower cells */
		    if (height > ((z + 1) * tbres + bottom)) {
			if (db.useLowerVal == 1)
			    value = inval;	/*Input map value */
			else
			    value = db.lower;
		    }
		    /*If exactly at the border, fill upper AND lower cell */
		    if (height >= (z * tbres + bottom) &&
			height <= ((z + 1) * tbres + bottom))
			value = inval;
		    /*If the elevation is null, set the G3D value null */
		    if (G3d_isNullValueNum(&height, DCELL_TYPE))
			value = null;

		    /*Write the value to the 3D map */
		    if (G3d_putDouble(db.map, x, y, z, value) < 0)
			fatal_error(db, _("Error writing 3D raster double data"));
		}
	    }
	    else {
		/*Use this method for every following 3d raster maps access */
		for (z = 0; z < depths; z++) {
		    /*Upper cells */
		    if (height < (z * tbres + bottom)) {
			if (db.useUpperVal == 1)
			    value = inval;	/*Input map value */
			else if (db.useUpperVal == 2)
			    value = db.upper;
			else
			    value = G3d_getDouble(db.map, x, y, z);
		    }
		    /*lower cells */
		    if (height > ((z + 1) * tbres + bottom)) {
			if (db.useLowerVal == 1)
			    value = inval;	/*Input map value */
			else if (db.useLowerVal == 2)
			    value = db.lower;
			else
			    value = G3d_getDouble(db.map, x, y, z);
		    }
		    /*If exactly at the border, fill upper AND lower cell */
		    if (height >= (z * tbres + bottom) &&
			height <= ((z + 1) * tbres + bottom))
			value = inval;
		    /*If the elevation is null, set the G3D value null */
		    if (G3d_isNullValueNum(&height, DCELL_TYPE))
			value = G3d_getDouble(db.map, x, y, z);

		    /*Write the value to the 3D map */
		    if (G3d_putDouble(db.map, x, y, z, value) < 0)
			fatal_error(db, _("Error writing 3D raster double data"));

		}
	    }
	}
    }

    if (input_rast)
	G_free(input_rast);
    if (elev_rast)
	G_free(elev_rast);

    return;
}

/* ************************************************************************* */
/* Main function, open the raster maps and create the G3D raster maps ****** */
/* ************************************************************************* */
int main(int argc, char *argv[])
{
    G3D_Region region;
    struct Cell_head window2d;
    struct GModule *module;
    int cols, rows, i;
    char *name = NULL;
    char *mapset = NULL;
    int changemask = 0;
    Database db;

    /*Initiate the database structure */
    db.map = NULL;
    db.input = 0;
    db.elev = 0;
    db.useUpperVal = 0;
    db.useLowerVal = 0;

    /* Initialize GRASS */
    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, raster3d, voxel, conversion");
    module->description =
	_("Creates a 3D volume map based on 2D elevation and value raster maps.");

    /* Get parameters from user */
    set_params();

    /* Have GRASS get inputs */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /*Check if maps exist */
    check_input_maps(&db);

    /*Do not use values */
    db.useUpperVal = 0;
    db.useLowerVal = 0;

    /*Use the input map value to fill the upper cells */
    if (param.fillup->answer) {
	db.useUpperVal = 1;
    }

    /*Use the input map value to fill the lower cells */
    if (param.filllow->answer) {
	db.useLowerVal = 1;
    }

    /*Set the upper value */
    if (param.upper->answer) {
	if (sscanf(param.upper->answer, "%lf", &db.upper))
	    db.useUpperVal = 2;
	else
	    G_fatal_error(_("The upper value is not valid"));
    }
    else {
	G3d_setNullValue(&db.upper, 1, DCELL_TYPE);
    }

    /*Set the lower value */
    if (param.lower->answer) {
	if (sscanf(param.lower->answer, "%lf", &db.lower))
	    db.useLowerVal = 2;
	else
	    G_fatal_error(_("The lower value is not valid"));
    }
    else {
	G3d_setNullValue(&db.lower, 1, DCELL_TYPE);
    }

    /* Figure out the current g3d region */
    G3d_initDefaults();
    G3d_getWindow(&region);

    /*Check if the g3d-region is equal to the 2d rows and cols */
    rows = G_window_rows();
    cols = G_window_cols();

    G_debug(2, "Checking 2d and 3d region");

    /*If not equal, set the 2D windows correct */
    if (rows != region.rows || cols != region.cols) {
	G_message(_("The 2D and 3D region settings are different. I will use the 3D region settings to adjust the 2D region."));
	G_get_set_window(&window2d);
	window2d.ns_res = region.ns_res;
	window2d.ew_res = region.ew_res;
	window2d.rows = region.rows;
	window2d.cols = region.cols;
	G_set_window(&window2d);
    }

    if (G_legal_filename(param.output->answer) < 0)
	G3d_fatalError(_("<%s> is an illegal file name"),
		       param.output->answer);

    G_debug(2, "Open 3d raster map %s", param.output->answer);

    /*open G3D output map */
    db.map = NULL;
    db.map =
	G3d_openCellNew(param.output->answer, DCELL_TYPE,
			G3D_USE_CACHE_DEFAULT, &region);
    if (db.map == NULL)
	fatal_error(db, _("Error opening 3d raster map"));


    /*if requested set the Mask on */
    if (param.mask->answer) {
	if (G3d_maskFileExists()) {
	    changemask = 0;
	    if (G3d_maskIsOff(db.map)) {
		G3d_maskOn(db.map);
		changemask = 1;
	    }
	}
    }


    G_message(_("Creating 3D raster map"));


    /*For each elevation - input map couple */
    for (i = 0; i < db.mapnum; i++) {

	G_debug(2, "Open input raster map %s", param.input->answers[i]);

	db.count = i;
	/*Open input map */
	mapset = NULL;
	name = NULL;
	name = param.input->answers[i];
	mapset = G_find_cell2(name, "");
	db.input = open_input_raster_map(name, mapset);
	db.inputmaptype = G_raster_map_type(name, mapset);

	G_debug(2, "Open elev raster map %s", param.elev->answers[i]);

	/*Open elev map */
	mapset = NULL;
	name = NULL;
	name = param.elev->answers[i];
	mapset = G_find_cell2(name, "");
	db.elev = open_input_raster_map(name, mapset);
	db.elevmaptype = G_raster_map_type(name, mapset);

	/****************************************/
	/*Write the data into the G3D Rastermap */
	elev_raster_to_g3d(db, region);

       /*****************************************/

	/* Close files */
	close_input_raster_map(db.input);
	close_input_raster_map(db.elev);
    }

    /*We set the Mask off, if it was off before */
    if (param.mask->answer) {
	if (G3d_maskFileExists())
	    if (G3d_maskIsOn(db.map) && changemask)
		G3d_maskOff(db.map);
    }

    G_debug(2, "Close 3d raster map");

    if (!G3d_closeCell(db.map))
	G3d_fatalError(_("Error closing 3d raster map"));

    G_debug(2, "\nDone\n");

    return (EXIT_SUCCESS);
}
