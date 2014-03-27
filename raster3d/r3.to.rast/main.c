
/****************************************************************************
*
* MODULE:       r3.to.rast 
*   	    	
* AUTHOR(S):    Original author 
*               Soeren Gebbert soerengebbert@gmx.de
* 		08 01 2005 Berlin
* PURPOSE:      Converts 3D raster maps to 2D raster maps  
*
* COPYRIGHT:    (C) 2005 by the GRASS Development Team
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


/*- Parameters and global variables -----------------------------------------*/
typedef struct
{
    struct Option *input, *output;
    struct Flag *mask;
    struct Flag *res;		/*If set, use the same resolution as the input map */
} paramType;

paramType param;		/*Parameters */

/*- prototypes --------------------------------------------------------------*/
void fatal_error(void *map, int *fd, int depths, char *errorMsg);	/*Simple Error message */
void set_params();		/*Fill the paramType structure */
void g3d_to_raster(void *map, G3D_Region region, int *fd);	/*Write the raster */
int open_output_map(const char *name, int res_type);	/*opens the outputmap */
void close_output_map(int fd);	/*close the map */



/* ************************************************************************* */
/* Error handling ********************************************************** */
/* ************************************************************************* */
void fatal_error(void *map, int *fd, int depths, char *errorMsg)
{
    int i;

    /* Close files and exit */
    if (map != NULL) {
	if (!G3d_closeCell(map))
	    G3d_fatalError(_("Unable to close 3D raster map"));
    }

    if (fd != NULL) {
	for (i = 0; i < depths; i++)
	    G_unopen_cell(fd[i]);
    }

    G3d_fatalError(errorMsg);
    exit(EXIT_FAILURE);

}

/* ************************************************************************* */
/* Set up the arguments we are expecting ********************************** */
/* ************************************************************************* */
void set_params()
{
    param.input = G_define_option();
    param.input->key = "input";
    param.input->type = TYPE_STRING;
    param.input->required = YES;
    param.input->gisprompt = "old,grid3,3d-raster";
    param.input->description =
	_("3D raster map(s) to be converted to 2D raster slices");

    param.output = G_define_option();
    param.output->key = "output";
    param.output->type = TYPE_STRING;
    param.output->required = YES;
    param.output->description = _("Basename for resultant raster slice maps");
    param.output->gisprompt = "new,cell,raster";

    param.mask = G_define_flag();
    param.mask->key = 'm';
    param.mask->description = _("Use 3D raster mask (if exists) with input map");

    param.res = G_define_flag();
    param.res->key = 'r';
    param.res->description =
	_("Use the same resolution as the input 3D raster map for the 2D output"
	  "maps, independent of the current region settings");
}

/* ************************************************************************* */
/* Write the slices to seperate raster maps ******************************** */
/* ************************************************************************* */
void g3d_to_raster(void *map, G3D_Region region, int *fd)
{
    double d1 = 0, f1 = 0;
    int x, y, z, check = 0;
    int rows, cols, depths, typeIntern, pos = 0;
    FCELL *fcell = NULL;
    DCELL *dcell = NULL;

    rows = region.rows;
    cols = region.cols;
    depths = region.depths;


    G_debug(2, "g3d_to_raster: Writing %i raster maps with %i rows %i cols.",
	    depths, rows, cols);

    typeIntern = G3d_tileTypeMap(map);

    if (typeIntern == FCELL_TYPE)
	fcell = G_allocate_f_raster_buf();
    else if (typeIntern == DCELL_TYPE)
	dcell = G_allocate_d_raster_buf();

    pos = 0;
    /*Every Rastermap */
    for (z = 0; z < depths; z++) {	/*From the bottom to the top */
	G_debug(2, "Writing raster map %d of %d", z + 1, depths);
	for (y = 0; y < rows; y++) {
	    G_percent(y, rows - 1, 10);

	    for (x = 0; x < cols; x++) {
		if (typeIntern == FCELL_TYPE) {
		    G3d_getValue(map, x, y, z, &f1, typeIntern);
		    if (G3d_isNullValueNum(&f1, FCELL_TYPE))
			G_set_null_value(&fcell[x], 1, FCELL_TYPE);
		    else
			fcell[x] = (FCELL) f1;
		}
		else {
		    G3d_getValue(map, x, y, z, &d1, typeIntern);
		    if (G3d_isNullValueNum(&d1, DCELL_TYPE))
			G_set_null_value(&dcell[x], 1, DCELL_TYPE);
		    else
			dcell[x] = (DCELL) d1;
		}
	    }
	    if (typeIntern == FCELL_TYPE) {
		check = G_put_f_raster_row(fd[pos], fcell);
		if (check != 1)
		    fatal_error(map, fd, depths,
				_("Unable to write raster row"));
	    }

	    if (typeIntern == DCELL_TYPE) {
		check = G_put_d_raster_row(fd[pos], dcell);
		if (check != 1)
		    fatal_error(map, fd, depths,
				_("Unable to write raster row"));
	    }
	}
	G_debug(2, "Finished writing map %d.", z + 1);
	pos++;
    }


    if (dcell)
	G_free(dcell);
    if (fcell)
	G_free(fcell);

}

/* ************************************************************************* */
/* Open the raster output map ********************************************** */
/* ************************************************************************* */
int open_output_map(const char *name, int res_type)
{
    int fd;

    fd = G_open_raster_new((char *)name, res_type);
    if (fd < 0)
	G_fatal_error(_("Unable to create raster map <%s>"), name);

    return fd;
}

/* ************************************************************************* */
/* Close the raster output map ********************************************* */
/* ************************************************************************* */
void close_output_map(int fd)
{
    if (G_close_cell(fd) < 0)
	G_fatal_error(_("Unable to close output map"));
}

/* ************************************************************************* */
/* Main function, open the G3D map and create the raster maps ************** */
/* ************************************************************************* */
int main(int argc, char *argv[])
{
    G3D_Region region, inputmap_bounds;
    struct Cell_head region2d;
    struct GModule *module;
    struct History history;
    void *map = NULL;		/*The 3D Rastermap */
    int i = 0, changemask = 0;
    int *fd = NULL, output_type, cols, rows;
    char *RasterFileName;

    /* Initialize GRASS */
    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster3d, voxel");
    module->description = _("Converts 3D raster maps to 2D raster maps");

    /* Get parameters from user */
    set_params();

    /* Have GRASS get inputs */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    G_debug(3, _("Open 3D raster map <%s>"), param.input->answer);

    if (NULL == G_find_grid3(param.input->answer, ""))
	G3d_fatalError(_("3D raster map <%s> not found"),
		       param.input->answer);

    /*Set the defaults */
    G3d_initDefaults();

    /*Set the resolution of the output maps */
    if (param.res->answer) {

	/*Open the map with default region */
	map = G3d_openCellOld(param.input->answer,
			      G_find_grid3(param.input->answer, ""),
			      G3D_DEFAULT_WINDOW, G3D_TILE_SAME_AS_FILE,
			      G3D_USE_CACHE_DEFAULT);
	if (map == NULL)
	    G3d_fatalError(_("Error opening 3D raster map <%s>"),
			   param.input->answer);


	/*Get the region of the map */
	G3d_getRegionStructMap(map, &region);
	/*set this region as current 3D window for map */
	G3d_setWindowMap(map, &region);
	/*Set the 2d region appropriate */
	G3d_extract2dRegion(&region, &region2d);
	/*Make the new 2d region the default */
	G_set_window(&region2d);

    }
    else {
	/* Figure out the region from the map */
	G3d_getWindow(&region);

	/*Check if the g3d-region is equal to the 2d rows and cols */
	rows = G_window_rows();
	cols = G_window_cols();

	/*If not equal, set the 3D window correct */
	if (rows != region.rows || cols != region.cols) {
	    G_message(_("The 2D and 3D region settings are different. "
			"Using the 2D window settings to adjust the 2D part of the 3D region."));
	    G_get_set_window(&region2d);
	    region.ns_res = region2d.ns_res;
	    region.ew_res = region2d.ew_res;
	    region.rows = region2d.rows;
	    region.cols = region2d.cols;
	    G3d_setWindow(&region);
	}

	/*Open the 3D raster map */
	map = G3d_openCellOld(param.input->answer,
			      G_find_grid3(param.input->answer, ""),
			      &region, G3D_TILE_SAME_AS_FILE,
			      G3D_USE_CACHE_DEFAULT);

	if (map == NULL)
	    G3d_fatalError(_("Error opening 3D raster map <%s>"),
			   param.input->answer);
    }

    /* save the input map region for later use (history meta-data) */
    G3d_getRegionStructMap(map, &inputmap_bounds);

    /*Get the output type */
    output_type = G3d_fileTypeMap(map);


    /*prepare the filehandler */
    fd = (int *)G_malloc(region.depths * sizeof(int));

    if (fd == NULL)
	fatal_error(map, NULL, 0, _("Out of memory"));

    if (G_legal_filename(param.output->answer) < 0)
	fatal_error(map, NULL, 0, _("Illegal output file name"));

    G_message(_("Creating %i raster maps"), region.depths);

    /*Loop over all output maps! open */
    for (i = 0; i < region.depths; i++) {
	/*Create the outputmaps */
	G_asprintf(&RasterFileName, "%s_%05d", param.output->answer, i + 1);
	G_message(_("Raster map %i Filename: %s"), i + 1, RasterFileName);

	if (G_find_cell2(RasterFileName, ""))
	    G_message(_("Raster map %d Filename: %s already exists. Will be overwritten!"),
		      i + 1, RasterFileName);

	if (output_type == FCELL_TYPE)
	    fd[i] = open_output_map(RasterFileName, FCELL_TYPE);
	else if (output_type == DCELL_TYPE)
	    fd[i] = open_output_map(RasterFileName, DCELL_TYPE);

    }

    /*if requested set the Mask on */
    if (param.mask->answer) {
	if (G3d_maskFileExists()) {
	    changemask = 0;
	    if (G3d_maskIsOff(map)) {
		G3d_maskOn(map);
		changemask = 1;
	    }
	}
    }

    /*Create the Rastermaps */
    g3d_to_raster(map, region, fd);


    /*Loop over all output maps! close */
    for (i = 0; i < region.depths; i++) {
	close_output_map(fd[i]);

	/* write history */
	G_asprintf(&RasterFileName, "%s_%i", param.output->answer, i + 1);
	G_debug(4, "Raster map %d Filename: %s", i + 1, RasterFileName);
	G_short_history(RasterFileName, "raster", &history);

	sprintf(history.datsrc_1, "3D Raster map:");
	strncpy(history.datsrc_2, param.input->answer, RECORD_LEN);
	history.datsrc_2[RECORD_LEN - 1] = '\0';	/* strncpy() doesn't null terminate if maxfill */

	sprintf(history.edhist[0], "Level %d of %d", i + 1, region.depths);
	sprintf(history.edhist[1], "Level z-range: %f to %f",
		region.bottom + (i * region.tb_res),
		region.bottom + (i + 1 * region.tb_res));

	sprintf(history.edhist[3], "Input map full z-range: %f to %f",
		inputmap_bounds.bottom, inputmap_bounds.top);
	sprintf(history.edhist[4], "Input map z-resolution: %f",
		inputmap_bounds.tb_res);
	history.edlinecnt = 5;
	if (!param.res->answer) {
	    sprintf(history.edhist[6], "GIS region full z-range: %f to %f",
		    region.bottom, region.top);
	    sprintf(history.edhist[7], "GIS region z-resolution: %f",
		    region.tb_res);
	    history.edlinecnt = 8;
	}

	G_command_history(&history);
	G_write_history(RasterFileName, &history);
    }

    /*We set the Mask off, if it was off before */
    if (param.mask->answer) {
	if (G3d_maskFileExists())
	    if (G3d_maskIsOn(map) && changemask)
		G3d_maskOff(map);
    }


    /*Cleaning */
    if (RasterFileName)
	G_free(RasterFileName);

    if (fd)
	G_free(fd);

    /* Close files and exit */
    if (!G3d_closeCell(map))
	fatal_error(map, NULL, 0, _("Unable to close 3D raster map"));

    map = NULL;

    return (EXIT_SUCCESS);
}
