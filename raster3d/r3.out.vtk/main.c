
/****************************************************************************
*
* MODULE:       r3.out.vtk  
*   	    	
* AUTHOR(S):    Original author 
*               Soeren Gebbert soerengebbert at gmx de
* 		27 Feb 2006 Berlin
* PURPOSE:      Converts 3D raster maps (G3D) into the VTK-Ascii format  
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

#define MAIN
#include "globalDefs.h"
#include "parameters.h"


#include "writeVTKData.h"
#include "writeVTKHead.h"
#include "errorHandling.h"

/** prototypes ***************************************************************/

/*Open the rgb voxel maps and write the data to the output */
static void open_write_rgb_maps(input_maps * in, G3D_Region region, FILE * fp,
				int dp);

/*Open the rgb voxel maps and write the data to the output */
static void open_write_vector_maps(input_maps * in, G3D_Region region,
				   FILE * fp, int dp);

/*opens a raster input map */
static int open_input_map(char *name, char *mapset);

/*Check if all maps are available */
static void check_input_maps(void);

/*Initiate the input maps structure */
static input_maps *create_input_maps_struct(void);



/* ************************************************************************* */
/* Open the raster input map *********************************************** */
/* ************************************************************************* */
input_maps *create_input_maps_struct(void)
{
    input_maps *in;

    in = (input_maps *) calloc(1, sizeof(input_maps));

    in->map = NULL;
    in->map_r = NULL;
    in->map_g = NULL;
    in->map_b = NULL;
    in->map_x = NULL;
    in->map_y = NULL;
    in->map_z = NULL;

    in->top = -1;
    in->bottom = -1;

    in->topMapType = 0;
    in->bottomMapType = 0;

    in->elevmaps = NULL;
    in->elevmaptypes = NULL;
    in->numelevmaps = 0;

    return in;
}

/* ************************************************************************* */
/* Open the raster input map *********************************************** */
/* ************************************************************************* */
int open_input_map(char *name, char *mapset)
{
    int fd;

    G_debug(3, "Open raster file %s in mapset %s", name, mapset);


    /* open raster map */
    fd = G_open_cell_old(name, mapset);

    if (fd < 0)
	G_fatal_error(_("Could not open map %s"), name);


    return fd;
}


/* ************************************************************************* */
/* Check the input maps **************************************************** */
/* ************************************************************************* */
void check_input_maps(void)
{
    int i = 0;
    char *mapset, *name;

    /*Check top and bottom if surface is requested */
    if (param.structgrid->answer) {

	if (!param.top->answer || !param.bottom->answer)
	    G3d_fatalError(_("Specify top and bottom map"));

	mapset = NULL;
	name = NULL;
	name = param.top->answer;
	mapset = G_find_cell2(name, "");
	if (mapset == NULL) {
	    G3d_fatalError(_("Top cell map <%s> not found"),
			   param.top->answer);
	}

	mapset = NULL;
	name = NULL;
	name = param.bottom->answer;
	mapset = G_find_cell2(name, "");
	if (mapset == NULL) {
	    G3d_fatalError(_("Bottom cell map <%s> not found"),
			   param.bottom->answer);
	}
    }

    /*If input maps are provided, check them */
    if (param.input->answers != NULL) {
	for (i = 0; param.input->answers[i] != NULL; i++) {
	    if (NULL == G_find_grid3(param.input->answers[i], ""))
		G3d_fatalError(_("3D raster map <%s> not found"),
			       param.input->answers[i]);
	}
    }

    /*Check for rgb maps. */
    if (param.rgbmaps->answers != NULL) {
	for (i = 0; i < 3; i++) {
	    if (param.rgbmaps->answers[i] != NULL) {
		if (NULL == G_find_grid3(param.rgbmaps->answers[i], ""))
		    G3d_fatalError(_("3D raster map <%s> not found"),
				   param.rgbmaps->answers[i]);
	    }
	    else {
		G3d_fatalError(_("Please provide three RGB 3D raster maps"));
	    }
	}
    }

    /*Check for vector maps. */
    if (param.vectormaps->answers != NULL) {
	for (i = 0; i < 3; i++) {
	    if (param.vectormaps->answers[i] != NULL) {
		if (NULL == G_find_grid3(param.vectormaps->answers[i], ""))
		    G3d_fatalError(_("3D vector map <%s> not found"),
				   param.vectormaps->answers[i]);
	    }
	    else {
		G3d_fatalError(_("Please provide three 3D raster maps for the xyz-vector maps [x,y,z]"));
	    }
	}
    }

    if (param.input->answers == NULL && param.rgbmaps->answers == NULL &&
	param.vectormaps->answers == NULL) {
	G_warning(_("No 3D raster data, RGB or xyz-vector maps are provided. Will only write the geometry"));
    }

    return;
}


/* ************************************************************************* */
/* Prepare the VTK RGB voxel data for writing ****************************** */
/* ************************************************************************* */
void open_write_rgb_maps(input_maps * in, G3D_Region region, FILE * fp,
			 int dp)
{
    int i, changemask[3] = { 0, 0, 0 };
    void *maprgb = NULL;

    if (param.rgbmaps->answers != NULL) {

	/*Loop over all input maps! */
	for (i = 0; i < 3; i++) {
	    G_debug(3, "Open RGB 3D raster map <%s>",
		    param.rgbmaps->answers[i]);

	    maprgb = NULL;
	    /*Open the map */
	    maprgb =
		G3d_openCellOld(param.rgbmaps->answers[i],
				G_find_grid3(param.rgbmaps->answers[i], ""),
				&region, G3D_TILE_SAME_AS_FILE,
				G3D_USE_CACHE_DEFAULT);
	    if (maprgb == NULL) {
		G_warning(_("Unable to open 3D raster map <%s>"),
			  param.rgbmaps->answers[i]);
		fatal_error(_("No RGB Data will be created"), in);
	    }

	    /*if requested set the Mask on */
	    if (param.mask->answer) {
		if (G3d_maskFileExists()) {
		    changemask[i] = 0;
		    if (G3d_maskIsOff(maprgb)) {
			G3d_maskOn(maprgb);
			changemask[i] = 1;
		    }
		}
	    }

	    if (i == 0)
		in->map_r = maprgb;
	    if (i == 1)
		in->map_g = maprgb;
	    if (i == 2)
		in->map_b = maprgb;
	}


	G_debug(3, "Writing VTK VoxelData");
	write_vtk_rgb_data(in->map_r, in->map_g, in->map_b, fp, "RGB_Voxel",
			   region, dp);

	for (i = 0; i < 3; i++) {
	    if (i == 0)
		maprgb = in->map_r;
	    if (i == 1)
		maprgb = in->map_g;
	    if (i == 2)
		maprgb = in->map_b;

	    /*We set the Mask off, if it was off before */
	    if (param.mask->answer) {
		if (G3d_maskFileExists())
		    if (G3d_maskIsOn(maprgb) && changemask[i])
			G3d_maskOff(maprgb);
	    }
	    /* Close the 3d raster map */
	    if (!G3d_closeCell(maprgb)) {
		fatal_error(_("Unable to close 3D raster map"), in);
	    }

	    /*Set the pointer to null so we noe later that these files are already closed */
	    if (i == 0)
		in->map_r = NULL;
	    if (i == 1)
		in->map_g = NULL;
	    if (i == 2)
		in->map_b = NULL;
	}
    }
    return;
}

/* ************************************************************************* */
/* Prepare the VTK vector data for writing ********************************* */
/* ************************************************************************* */
void open_write_vector_maps(input_maps * in, G3D_Region region, FILE * fp,
			    int dp)
{
    int i, changemask[3] = { 0, 0, 0 };
    void *mapvect = NULL;

    if (param.vectormaps->answers != NULL) {

	/*Loop over all input maps! */
	for (i = 0; i < 3; i++) {
	    G_debug(3, "Open vector 3D raster map <%s>",
		    param.vectormaps->answers[i]);

	    mapvect = NULL;
	    /*Open the map */
	    mapvect =
		G3d_openCellOld(param.vectormaps->answers[i],
				G_find_grid3(param.vectormaps->answers[i],
					     ""), &region,
				G3D_TILE_SAME_AS_FILE, G3D_USE_CACHE_DEFAULT);
	    if (mapvect == NULL) {
		G_warning(_("Unable to open 3D raster map <%s>"),
			  param.vectormaps->answers[i]);
		fatal_error(_("No vector data will be created"), in);
	    }

	    /*if requested set the Mask on */
	    if (param.mask->answer) {
		if (G3d_maskFileExists()) {
		    changemask[i] = 0;
		    if (G3d_maskIsOff(mapvect)) {
			G3d_maskOn(mapvect);
			changemask[i] = 1;
		    }
		}
	    }

	    if (i == 0)
		in->map_x = mapvect;
	    if (i == 1)
		in->map_y = mapvect;
	    if (i == 2)
		in->map_z = mapvect;
	}


	G_debug(3, "Writing VTK Vector Data");
	write_vtk_vector_data(in->map_x, in->map_y, in->map_z, fp,
			      "Vector_Data", region, dp);

	for (i = 0; i < 3; i++) {
	    if (i == 0)
		mapvect = in->map_x;
	    if (i == 1)
		mapvect = in->map_y;
	    if (i == 2)
		mapvect = in->map_z;

	    /*We set the Mask off, if it was off before */
	    if (param.mask->answer) {
		if (G3d_maskFileExists())
		    if (G3d_maskIsOn(mapvect) && changemask[i])
			G3d_maskOff(mapvect);
	    }

	    /* Close the 3d raster map */
	    if (!G3d_closeCell(mapvect)) {
		fatal_error(_("Unable to close 3D raster map"), in);
	    }
	    /*Set the pointer to null so we know later that these files are already closed */
	    if (i == 0)
		in->map_x = NULL;
	    if (i == 1)
		in->map_y = NULL;
	    if (i == 2)
		in->map_z = NULL;
	}
    }
    return;
}


/* ************************************************************************* */
/* Main function, opens most of the input and output files ***************** */
/* ************************************************************************* */
int main(int argc, char *argv[])
{
    char *output = NULL;
    G3D_Region region;
    struct Cell_head window2d;
    struct Cell_head default_region;
    FILE *fp = NULL;
    struct GModule *module;
    int dp, i, changemask = 0;
    int rows, cols;
    char *mapset, *name;
    double scale = 1.0, llscale = 1.0;

    input_maps *in;

    /* Initialize GRASS */
    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster3d, voxel, export");
    module->description =
	_("Converts 3D raster maps into the VTK-ASCII format.");

    /* Get parameters from user */
    set_params();

    /* Have GRASS get inputs */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);
    /*The precision of the output */
    if (param.decimals->answer) {
	if (sscanf(param.decimals->answer, "%d", &dp) != 1)
	    G_fatal_error(_("failed to interpret dp as an integer"));
	if (dp > 20 || dp < 0)
	    G_fatal_error(_("dp has to be from 0 to 20"));
    }
    else {
	dp = 8;			/*This value is taken from the lib settings in G_format_easting */
    }

    /*Check the input */
    check_input_maps();

    /*Correct the coordinates, so the precision of VTK is not hurt :( */
    if (param.coorcorr->answer) {
	/*Get the default region for coordiante correction */
	G_get_default_window(&default_region);

	/*Use the center of the current region as extent */
	y_extent = (default_region.north + default_region.south) / 2;
	x_extent = (default_region.west + default_region.east) / 2;
    }
    else {
	x_extent = 0;
	y_extent = 0;
    }

    /*open the output */
    if (param.output->answer) {
	fp = fopen(param.output->answer, "w");
	if (fp == NULL) {
	    perror(param.output->answer);
	    G_usage();
	    exit(EXIT_FAILURE);
	}
    }
    else
	fp = stdout;

    /* Figure out the region from the map */
    G3d_initDefaults();
    G3d_getWindow(&region);

    /*initiate the input mpas structure */
    in = create_input_maps_struct();


    /* read and compute the scale factor */
    sscanf(param.elevscale->answer, "%lf", &scale);
    /*if LL projection, convert the elevation values to degrees */
    if (region.proj == PROJECTION_LL) {
	llscale = M_PI / (180) * 6378137;
	scale /= llscale;
    }

    /*Open the top and bottom file */
    if (param.structgrid->answer) {

	/*Check if the g3d-region is equal to the 2d rows and cols */
	rows = G_window_rows();
	cols = G_window_cols();

	/*If not equal, set the 2D windows correct */
	if (rows != region.rows || cols != region.cols) {
	    G_message(_("The 2D and 3D region settings are different. "
			"Using the 2D window settings to adjust the 2D part of the 3D region."));
	    G_get_set_window(&window2d);
	    window2d.ns_res = region.ns_res;
	    window2d.ew_res = region.ew_res;
	    window2d.rows = region.rows;
	    window2d.cols = region.cols;
	    G_set_window(&window2d);
	}

	/*open top */
	mapset = NULL;
	name = NULL;
	name = param.top->answer;
	mapset = G_find_cell2(name, "");
	in->top = open_input_map(name, mapset);
	in->topMapType = G_get_raster_map_type(in->top);

	/*open bottom */
	mapset = NULL;
	name = NULL;
	name = param.bottom->answer;
	mapset = G_find_cell2(name, "");
	in->bottom = open_input_map(name, mapset);
	in->bottomMapType = G_get_raster_map_type(in->bottom);

	/* Write the vtk-header and the points */
	if (param.point->answer) {
	    write_vtk_structured_grid_header(fp, output, region);
	    write_vtk_points(in, fp, region, dp, 1, scale);
	}
	else {
	    write_vtk_unstructured_grid_header(fp, output, region);
	    write_vtk_points(in, fp, region, dp, 0, scale);
	    write_vtk_unstructured_grid_cells(fp, region);
	}

	if (G_close_cell(in->top) < 0) {
	    G_fatal_error(_("Unable to close top raster map"));
	}
	in->top = -1;

	if (G_close_cell(in->bottom) < 0) {
	    G_fatal_error(_("Unable to close bottom raster map"));
	}
	in->bottom = -1;
    }
    else {
	/* Write the structured point vtk-header */
	write_vtk_structured_point_header(fp, output, region, dp, scale);
    }

    /*Write the normal VTK data (cell or point data) */
    /*Loop over all 3d input maps! */
    if (param.input->answers != NULL) {
	for (i = 0; param.input->answers[i] != NULL; i++) {

	    G_debug(3, "Open 3D raster map <%s>", param.input->answers[i]);

	    /*Open the map */
	    in->map =
		G3d_openCellOld(param.input->answers[i],
				G_find_grid3(param.input->answers[i], ""),
				&region, G3D_TILE_SAME_AS_FILE,
				G3D_USE_CACHE_DEFAULT);
	    if (in->map == NULL) {
		G_warning(_("Unable to open 3D raster map <%s>"),
			  param.input->answers[i]);
		fatal_error(" ", in);
	    }

	    /*if requested set the Mask on */
	    if (param.mask->answer) {
		if (G3d_maskFileExists()) {
		    changemask = 0;
		    if (G3d_maskIsOff(in->map)) {
			G3d_maskOn(in->map);
			changemask = 1;
		    }
		}
	    }

	    /* Write the point or cell data */
	    write_vtk_data(fp, in->map, region, param.input->answers[i], dp);

	    /*We set the Mask off, if it was off before */
	    if (param.mask->answer) {
		if (G3d_maskFileExists())
		    if (G3d_maskIsOn(in->map) && changemask)
			G3d_maskOff(in->map);
	    }

	    /* Close the 3d raster map */
	    if (!G3d_closeCell(in->map)) {
		in->map = NULL;
		fatal_error(_("Unable to close 3D raster map, the VTK file may be incomplete"),
			    in);
	    }

	    in->map = NULL;
	}
    }

    /*Write the RGB voxel data */
    open_write_rgb_maps(in, region, fp, dp);
    open_write_vector_maps(in, region, fp, dp);

    /*Close the output file */
    if (param.output->answer && fp != NULL)
	if (fclose(fp))
	    fatal_error(_("Unable to close VTK-ASCII file"), in);

    /*close all open maps and free memory */
    release_input_maps_struct(in);

    return 0;
}
