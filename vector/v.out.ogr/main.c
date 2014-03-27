/*
 ***************************************************************
 *
 * MODULE:       v.out.ogr
 *
 * AUTHOR(S):    Radim Blazek
 *               Some extensions: Markus Neteler, Benjamin Ducke
 *
 * PURPOSE:      Category manipulations
 *
 * COPYRIGHT:    (C) 2001-2009 by the GRASS Development Team
 *
 *               This program is free software under the
 *               GNU General Public License (>=v2).
 *               Read the file COPYING that comes with GRASS
 *               for details.
 *
 **************************************************************/
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/dbmi.h>
#include <grass/Vect.h>
#include <grass/config.h>
#include <grass/gprojects.h>
#include <grass/glocale.h>
#include "ogr_api.h"
#include "cpl_string.h"


/* some hard limits */
#define SQL_BUFFER_SIZE 2000
#define MAX_OGR_DRIVERS 2000


int fout, fskip;		/* features written/ skip */
int nocat, noatt, nocatskip;	/* number of features without cats/atts written/skip */

int mk_att(int cat, struct field_info *Fi, dbDriver * Driver,
	   int ncol, int doatt, int nocat, OGRFeatureH Ogr_feature);

char *OGR_list_write_drivers();
char OGRdrivers[MAX_OGR_DRIVERS];

int main(int argc, char *argv[])
{
    int i, j, k, centroid, otype, donocat;
    int num_to_export;
    char *mapset;
    int field;
    int overwrite, found;
    struct GModule *module;
    struct Option *in_opt, *dsn_opt, *layer_opt, *type_opt, *frmt_opt,
	*field_opt, *dsco, *lco;
    struct Flag *cat_flag, *esristyle, *poly_flag, *update_flag, *nocat_flag,
	*shapez_flag;
    char buf[SQL_BUFFER_SIZE];
    char key1[SQL_BUFFER_SIZE], key2[SQL_BUFFER_SIZE];
    struct Key_Value *projinfo, *projunits;
    struct Cell_head cellhd;
    char **tokens;

    /* Vector */
    struct Map_info In;
    struct line_pnts *Points;
    struct line_cats *Cats;
    int type, cat;

    /* Attributes */
    int doatt = 0, ncol = 0, colsqltype, colctype, colwidth, keycol = -1;
    struct field_info *Fi = NULL;
    dbDriver *Driver = NULL;
    dbHandle handle;
    dbTable *Table;
    dbString dbstring;
    dbColumn *Column;

    /* OGR */
    int drn, ogr_ftype = OFTInteger;
    OGRDataSourceH Ogr_ds;
    OGRSFDriverH Ogr_driver;
    OGRLayerH Ogr_layer;
    OGRFieldDefnH Ogr_field;
    OGRFeatureH Ogr_feature;
    OGRFeatureDefnH Ogr_featuredefn;
    OGRGeometryH Ogr_geometry;
    unsigned int wkbtype = wkbUnknown;	/* ?? */
    OGRSpatialReferenceH Ogr_projection;
    char **papszDSCO = NULL, **papszLCO = NULL;
    int num_types;

    G_gisinit(argv[0]);

    /* Module options */
    module = G_define_module();
    module->keywords = _("vector, export");
    module->description =
	_("Converts GRASS vector map to one of the supported OGR vector formats.");

    in_opt = G_define_standard_option(G_OPT_V_INPUT);

    type_opt = G_define_standard_option(G_OPT_V3_TYPE);
    type_opt->options = "point,line,boundary,centroid,area,face,kernel,auto";
    type_opt->answer = "line,boundary";
    type_opt->description =
	_("Feature type(s). Combinations not supported "
	  "by all output formats. Default: first type found in input.");
    type_opt->guisection = _("Input");

    dsn_opt = G_define_option();
    dsn_opt->key = "dsn";
    dsn_opt->type = TYPE_STRING;
    dsn_opt->required = YES;
    dsn_opt->label = _("OGR output datasource name");
    dsn_opt->description =
	_("For example: ESRI Shapefile: filename or directory for storage");

    layer_opt = G_define_option();
    layer_opt->key = "olayer";
    layer_opt->type = TYPE_STRING;
    layer_opt->required = NO;
    layer_opt->label =
	_("OGR layer name. If not specified, input name is used.");
    layer_opt->description = _("For example: ESRI Shapefile: shapefile name");
    layer_opt->guisection = _("Creation");

    field_opt = G_define_standard_option(G_OPT_V_FIELD);
    field_opt->guisection = _("Input");

    frmt_opt = G_define_option();
    frmt_opt->key = "format";
    frmt_opt->type = TYPE_STRING;
    frmt_opt->required = NO;
    frmt_opt->multiple = NO;
    frmt_opt->answer = "ESRI_Shapefile";
    frmt_opt->options = OGR_list_write_drivers();
    frmt_opt->description = _("OGR format");
    frmt_opt->guisection = _("Creation");

    dsco = G_define_option();
    dsco->key = "dsco";
    dsco->type = TYPE_STRING;
    dsco->required = NO;
    dsco->multiple = YES;
    dsco->answer = "";
    dsco->description =
	_("OGR dataset creation option (format specific, NAME=VALUE)");
    dsco->guisection = _("Creation");

    lco = G_define_option();
    lco->key = "lco";
    lco->type = TYPE_STRING;
    lco->required = NO;
    lco->multiple = YES;
    lco->answer = "";
    lco->description =
	_("OGR layer creation option (format specific, NAME=VALUE)");
    lco->guisection = _("Creation");

    update_flag = G_define_flag();
    update_flag->key = 'u';
    update_flag->description = _("Open an existing datasource for update");

    nocat_flag = G_define_flag();
    nocat_flag->key = 's';
    nocat_flag->description =
	_("Skip export of GRASS category ID ('cat') attribute");

    cat_flag = G_define_flag();
    cat_flag->key = 'c';
    cat_flag->description = _("Export features with category (labeled) only. "
			      "Otherwise all features are exported");

    esristyle = G_define_flag();
    esristyle->key = 'e';
    esristyle->description = _("Use ESRI-style .prj file format "
			       "(applies to Shapefile output only)");

    shapez_flag = G_define_flag();
    shapez_flag->key = 'z';
    shapez_flag->description = _("Create 3D output if input is 3D "
				 "(applies to Shapefile output only)");

    poly_flag = G_define_flag();
    poly_flag->key = 'p';
    poly_flag->description = _("Export lines as polygons");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* read options */
    field = atoi(field_opt->answer);

    /* open input vector */
    if ((mapset = G_find_vector2(in_opt->answer, "")) == NULL) {
	G_fatal_error(_("Vector map <%s> not found"), in_opt->answer);
    }

    Vect_set_open_level(2);
    Vect_open_old(&In, in_opt->answer, "");

    /*
       If no output type specified: determine one automatically.
       Centroids, Boundaries and Kernels always have to be
       exported explicitely, using the "type=" option.
     */
    if (!strcmp(type_opt->answer, "auto")) {
	G_debug(2, "Automatic type determination.");

	type_opt->answers = G_malloc(sizeof(char *) * 100);	/* should be big enough forever ;) */
	for (i = 0; i < 100; i++) {
	    type_opt->answers[i] = NULL;
	}
	num_types = 0;

	if (Vect_get_num_primitives(&In, GV_POINT) > 0) {
	    type_opt->answers[num_types] = strdup("point");
	    G_debug(3, "Adding points to export list.");
	    num_types++;
	}

	if (Vect_get_num_primitives(&In, GV_LINE) > 0) {
	    type_opt->answers[num_types] = strdup("line");
	    G_debug(3, "Adding lines to export list.");
	    num_types++;
	}

	if (Vect_get_num_primitives(&In, GV_BOUNDARY) !=
	    Vect_get_num_areas(&In)) {
	    G_warning(_("Skipping all boundaries that are not part of an area."));
	}

	if (Vect_get_num_areas(&In) > 0) {
	    type_opt->answers[num_types] = strdup("area");
	    G_debug(3, "Adding areas to export list.");
	    num_types++;
	}

	/*  Faces and volumes:
	   For now, volumes will just be exported as sets of faces.
	 */
	if (Vect_get_num_primitives(&In, GV_FACE) > 0) {
	    type_opt->answers[num_types] = strdup("face");
	    G_debug(3, "Adding faces to export list.");
	    num_types++;
	}
	/* this check HAS TO FOLLOW RIGHT AFTER check for GV_FACE! */
	if (Vect_get_num_volumes(&In) > 0) {
	    G_warning(_("Volumes will be exported as sets of faces."));
	    if (num_types == 0) {
		/* no other types yet? */
		type_opt->answers[num_types] = strdup("volume");
		G_debug(3, "Adding volumes to export list.");
		num_types++;
	    }
	    else {
		if (strcmp(type_opt->answers[num_types - 1], "face")) {
		    /* only put faces on export list if that's not the case already */
		    type_opt->answers[num_types] = strdup("volume");
		    G_debug(3, "Adding volumes to export list.");
		    num_types++;
		}
	    }
	}

	if (num_types == 0)
	    G_fatal_error(_("Could not determine input map's feature type(s)."));
    }

    /* Check output type */
    otype = Vect_option_to_types(type_opt);

    if (!layer_opt->answer) {
	char xname[GNAME_MAX], xmapset[GMAPSET_MAX];

	if (G__name_is_fully_qualified(in_opt->answer, xname, xmapset))
	    layer_opt->answer = G_store(xname);
	else
	    layer_opt->answer = G_store(in_opt->answer);
    }

    if (otype & GV_POINTS)
	wkbtype = wkbPoint;
    else if (otype & GV_LINES)
	wkbtype = wkbLineString;
    else if (otype & GV_AREA)
	wkbtype = wkbPolygon;
    else if (otype & GV_FACE)
	wkbtype = wkbPolygon25D;
    else if (otype & GV_VOLUME)
	wkbtype = wkbPolygon25D;

    if (poly_flag->answer)
	wkbtype = wkbPolygon;

    if (((GV_POINTS & otype) && (GV_LINES & otype)) ||
	((GV_POINTS & otype) && (GV_AREA & otype)) ||
	((GV_POINTS & otype) && (GV_FACE & otype)) ||
	((GV_POINTS & otype) && (GV_KERNEL & otype)) ||
	((GV_POINTS & otype) && (GV_VOLUME & otype)) ||
	((GV_LINES & otype) && (GV_AREA & otype)) ||
	((GV_LINES & otype) && (GV_FACE & otype)) ||
	((GV_LINES & otype) && (GV_KERNEL & otype)) ||
	((GV_LINES & otype) && (GV_VOLUME & otype)) ||
	((GV_KERNEL & otype) && (GV_POINTS & otype)) ||
	((GV_KERNEL & otype) && (GV_LINES & otype)) ||
	((GV_KERNEL & otype) && (GV_AREA & otype)) ||
	((GV_KERNEL & otype) && (GV_FACE & otype)) ||
	((GV_KERNEL & otype) && (GV_VOLUME & otype))

	) {
	G_warning(_("The combination of types is not supported"
		    " by all formats."));
	wkbtype = wkbUnknown;
    }

    if (cat_flag->answer)
	donocat = 0;
    else
	donocat = 1;

    Points = Vect_new_line_struct();
    Cats = Vect_new_cats_struct();

    if (Vect_get_num_islands(&In) > 0 && !cat_flag->answer)
	G_warning(_("The map contains islands. To preserve them in the output map, use the -c flag"));


    /* check what users wants to export and what's present in the map */
    if (Vect_get_num_primitives(&In, GV_POINT) > 0 && !(otype & GV_POINTS))
	G_warning(_("%d point(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									 GV_POINT));

    if (Vect_get_num_primitives(&In, GV_LINE) > 0 && !(otype & GV_LINES))
	G_warning(_("%d line(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									 GV_LINE));

    if (Vect_get_num_primitives(&In, GV_BOUNDARY) > 0 &&
	!(otype & GV_BOUNDARY) && !(otype & GV_AREA))
	G_warning(_("%d boundary(ies) found, but not requested to be exported. "
		   "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									GV_BOUNDARY));

    if (Vect_get_num_primitives(&In, GV_CENTROID) > 0 &&
	!(otype & GV_CENTROID) && !(otype & GV_AREA))
	G_warning(_("%d centroid(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									 GV_CENTROID));

    if (Vect_get_num_areas(&In) > 0 && !(otype & GV_AREA))
	G_warning(_("%d areas found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_areas(&In));

    if (Vect_get_num_primitives(&In, GV_FACE) > 0 && !(otype & GV_FACE))
	G_warning(_("%d face(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									 GV_FACE));

    if (Vect_get_num_primitives(&In, GV_KERNEL) > 0 &&
	!(otype & GV_KERNEL) && !(otype & GV_VOLUME))
	G_warning(_("%d kernel(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_primitives(&In,
									 GV_KERNEL));

    if (Vect_get_num_volumes(&In) > 0 && !(otype & GV_VOLUME))
	G_warning(_("%d volume(s) found, but not requested to be exported. "
		    "Verify 'type' parameter."), Vect_get_num_volumes(&In));

    /* warn and eventually abort if there is nothing to be exported */
    num_to_export = 0;
    if (Vect_get_num_primitives(&In, GV_POINT) < 1 && (otype & GV_POINTS)) {
	G_warning(_("No points found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_POINT)
	    num_to_export += Vect_get_num_primitives(&In, GV_POINT);
    }

    if (Vect_get_num_primitives(&In, GV_LINE) < 1 && (otype & GV_LINE)) {
	G_warning(_("No lines found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_LINE)
	    num_to_export += Vect_get_num_primitives(&In, GV_LINE);
    }

    if (Vect_get_num_primitives(&In, GV_BOUNDARY) < 1 &&
	(otype & GV_BOUNDARY)) {
	G_warning(_("No boundaries found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_BOUNDARY)
	    num_to_export += Vect_get_num_primitives(&In, GV_BOUNDARY);
    }

    if (Vect_get_num_areas(&In) < 1 && (otype & GV_AREA)) {
	G_warning(_("No areas found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_AREA)
	    num_to_export += Vect_get_num_areas(&In);
    }

    if (Vect_get_num_primitives(&In, GV_CENTROID) < 1 &&
	(otype & GV_CENTROID)) {
	G_warning(_("No centroids found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_CENTROID)
	    num_to_export += Vect_get_num_primitives(&In, GV_CENTROID);
    }

    if (Vect_get_num_primitives(&In, GV_FACE) < 1 && (otype & GV_FACE)) {
	G_warning(_("No faces found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_FACE)
	    num_to_export += Vect_get_num_primitives(&In, GV_FACE);
    }

    if (Vect_get_num_primitives(&In, GV_KERNEL) < 1 && (otype & GV_KERNEL)) {
	G_warning(_("No kernels found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_KERNEL)
	    num_to_export += Vect_get_num_primitives(&In, GV_KERNEL);
    }

    if (Vect_get_num_volumes(&In) < 1 && (otype & GV_VOLUME)) {
	G_warning(_("No volumes found, but requested to be exported. "
		    "Will skip this geometry type."));
    }
    else {
	if (otype & GV_VOLUME)
	    num_to_export += Vect_get_num_volumes(&In);
    }

    G_debug(1, "Requested to export %d geometries", num_to_export);

    if (num_to_export < 1) {
	G_warning(_("Nothing to export"));
	exit(EXIT_SUCCESS);
    }


    /* fetch PROJ info */
    G_get_default_window(&cellhd);
    if (cellhd.proj == PROJECTION_XY)
	Ogr_projection = NULL;
    else {
	projinfo = G_get_projinfo();
	projunits = G_get_projunits();
	Ogr_projection = GPJ_grass_to_osr(projinfo, projunits);
	if (esristyle->answer &&
	    (strcmp(frmt_opt->answer, "ESRI_Shapefile") == 0))
	    OSRMorphToESRI(Ogr_projection);
    }

    /* Open OGR DSN */
    G_debug(2, "driver count = %d", OGRGetDriverCount());
    drn = -1;
    for (i = 0; i < OGRGetDriverCount(); i++) {
	Ogr_driver = OGRGetDriver(i);
	G_debug(2, "driver %d : %s", i, OGR_Dr_GetName(Ogr_driver));
	/* chg white space to underscore in OGR driver names */
	sprintf(buf, "%s", OGR_Dr_GetName(Ogr_driver));
	G_strchg(buf, ' ', '_');
	if (strcmp(buf, frmt_opt->answer) == 0) {
	    drn = i;
	    G_debug(2, " -> driver = %d", drn);
	}
    }
    if (drn == -1)
	G_fatal_error(_("OGR driver <%s> not found"), frmt_opt->answer);
    Ogr_driver = OGRGetDriver(drn);

    /* parse dataset creation options */
    i = 0;
    while (dsco->answers[i]) {
	tokens = G_tokenize(dsco->answers[i], "=");
	if (G_number_of_tokens(tokens))
	    papszDSCO = CSLSetNameValue(papszDSCO, tokens[0], tokens[1]);
	G_free_tokens(tokens);
	i++;
    }

    if (update_flag->answer) {
	G_debug(1, "Update OGR data source");
	Ogr_ds = OGR_Dr_Open(Ogr_driver, dsn_opt->answer, TRUE);
    }
    else {
	G_debug(1, "Create OGR data source");
	Ogr_ds =
	    OGR_Dr_CreateDataSource(Ogr_driver, dsn_opt->answer, papszDSCO);
    }

    CSLDestroy(papszDSCO);
    if (Ogr_ds == NULL)
	G_fatal_error(_("Unable to open OGR data source '%s'"),
		      dsn_opt->answer);

    /* check if OGR layer exists */
    overwrite = G_check_overwrite(argc, argv);
    found = FALSE;
    for (i = 0; i < OGR_DS_GetLayerCount(Ogr_ds); i++) {
	Ogr_layer = OGR_DS_GetLayer(Ogr_ds, i);
	Ogr_field = OGR_L_GetLayerDefn(Ogr_layer);
	if (strcmp(OGR_FD_GetName(Ogr_field), layer_opt->answer))
	    continue;

	found = TRUE;
	if (!overwrite) {
	    G_fatal_error(_("Layer <%s> already exists in OGR data source '%s'"),
			  layer_opt->answer, dsn_opt->answer);
	}
	else if (overwrite) {
	    G_warning(_("OGR layer <%s> already exists and will be overwritten"),
		      layer_opt->answer);
	    OGR_DS_DeleteLayer(Ogr_ds, i);
	    break;
	}
    }

    /* parse layer creation options */
    i = 0;
    while (lco->answers[i]) {
	tokens = G_tokenize(lco->answers[i], "=");
	if (G_number_of_tokens(tokens))
	    papszLCO = CSLSetNameValue(papszLCO, tokens[0], tokens[1]);
	G_free_tokens(tokens);
	i++;
    }

    /* Automatically append driver options for 3D output to
       layer creation options if 'z' is given. */
    if ((shapez_flag->answer) && (Vect_is_3d(&In)) &&
	(strcmp(frmt_opt->answer, "ESRI_Shapefile") == 0)) {
	/* find right option */
	char shape_geom[20];

	if ((otype & GV_POINTS) || (otype & GV_KERNEL))
	    sprintf(shape_geom, "POINTZ");
	if ((otype & GV_LINES))
	    sprintf(shape_geom, "ARCZ");
	if ((otype & GV_AREA) || (otype & GV_FACE) || (otype & GV_VOLUME))
	    sprintf(shape_geom, "POLYGONZ");
	/* check if the right LCO is already present */
	const char *shpt;

	shpt = CSLFetchNameValue(papszLCO, "SHPT");
	if ((!shpt)) {
	    /* Not set at all? Good! */
	    papszLCO = CSLSetNameValue(papszLCO, "SHPT", shape_geom);
	}
	else {
	    if (strcmp(shpt, shape_geom) != 0) {
		/* Set but to a different value? Override! */
		G_warning(_("Overriding existing user-defined 'SHPT=' LCO."));
	    }
	    /* Set correct LCO for this geometry type */
	    papszLCO = CSLSetNameValue(papszLCO, "SHPT", shape_geom);
	}
    }


    /* check if the map is 3d */
    if (Vect_is_3d(&In)) {
	/* specific check for shp */
	if (strcmp(frmt_opt->answer, "ESRI_Shapefile") == 0) {
	    const char *shpt;

	    shpt = CSLFetchNameValue(papszLCO, "SHPT");
	    if (!shpt || shpt[strlen(shpt) - 1] != 'Z') {
		G_warning(_("Vector map <%s> is 3D. "
			    "Use format specific layer creation options (parameter 'lco') "
			    "or '-z' flag to export in 3D rather than 2D (default)"),
			  in_opt->answer);
	    }
	}
	else {
	    G_warning(_("Vector map <%s> is 3D. "
			"Use format specific layer creation options (parameter 'lco') "
			"to export in 3D rather than 2D (default)"),
		      in_opt->answer);
	}
    }

    G_debug(1, "Create OGR layer");
    Ogr_layer =
	OGR_DS_CreateLayer(Ogr_ds, layer_opt->answer, Ogr_projection, wkbtype,
			   papszLCO);
    CSLDestroy(papszLCO);
    if (Ogr_layer == NULL)
	G_fatal_error(_("Unable to create OGR layer"));

    db_init_string(&dbstring);

    /* Vector attributes -> OGR fields */
    if (field > 0) {
	G_debug(1, "Create attribute table");
	doatt = 1;		/* do attributes */
	Fi = Vect_get_field(&In, field);
	if (Fi == NULL) {
	    G_warning(_("No attribute table found -> using only category numbers as attributes"));
	    /* if we have no more than a 'cat' column, then that has to
	       be exported in any case */
	    if (nocat_flag->answer) {
		G_warning(_("Exporting 'cat' anyway, as it is the only attribute table field"));
		nocat_flag->answer = 0;
	    }
	    Ogr_field = OGR_Fld_Create("cat", OFTInteger);
	    OGR_L_CreateField(Ogr_layer, Ogr_field, 0);
	    OGR_Fld_Destroy(Ogr_field);

	    doatt = 0;
	}
	else {
	    Driver = db_start_driver(Fi->driver);
	    if (Driver == NULL)
		G_fatal_error(_("Unable to start driver <%s>"), Fi->driver);

	    db_init_handle(&handle);
	    db_set_handle(&handle, Fi->database, NULL);
	    if (db_open_database(Driver, &handle) != DB_OK)
		G_fatal_error(_("Unable to open database <%s> by driver <%s>"),
			      Fi->database, Fi->driver);

	    db_set_string(&dbstring, Fi->table);
	    if (db_describe_table(Driver, &dbstring, &Table) != DB_OK)
		G_fatal_error(_("Unable to describe table <%s>"), Fi->table);

	    ncol = db_get_table_number_of_columns(Table);
	    G_debug(2, "ncol = %d", ncol);
	    keycol = -1;
	    for (i = 0; i < ncol; i++) {
		Column = db_get_table_column(Table, i);
		colsqltype = db_get_column_sqltype(Column);
		G_debug(2, "col %d: %s (%s)", i, db_get_column_name(Column),
			db_sqltype_name(colsqltype));
		colctype = db_sqltype_to_Ctype(colsqltype);
                colwidth = db_get_column_length(Column);
                G_debug(3, "col %d: %s sqltype=%d ctype=%d width=%d",
                        i, db_get_column_name(Column), colsqltype, 
                        colctype, colwidth);

		switch (colctype) {
		case DB_C_TYPE_INT:
		    ogr_ftype = OFTInteger;
		    break;
		case DB_C_TYPE_DOUBLE:
		    ogr_ftype = OFTReal;
		    break;
		case DB_C_TYPE_STRING:
		    ogr_ftype = OFTString;
		    break;
		case DB_C_TYPE_DATETIME:
		    ogr_ftype = OFTString;
		    break;
		}
		G_debug(2, "ogr_ftype = %d", ogr_ftype);

		strcpy(key1, Fi->key);
		G_tolcase(key1);
		strcpy(key2, db_get_column_name(Column));
		G_tolcase(key2);
		if (strcmp(key1, key2) == 0)
		    keycol = i;
		G_debug(2, "%s x %s -> %s x %s -> keycol = %d", Fi->key,
			db_get_column_name(Column), key1, key2, keycol);

		if (!nocat_flag->answer) {
		    Ogr_field =
			OGR_Fld_Create(db_get_column_name(Column), ogr_ftype);
                    if (ogr_ftype == OFTString && colwidth > 0) 
                        OGR_Fld_SetWidth(Ogr_field, colwidth);
		    OGR_L_CreateField(Ogr_layer, Ogr_field, 0);
		    OGR_Fld_Destroy(Ogr_field);
		}
		else {
		    /* skip export of 'cat' field */
		    if (strcmp(Fi->key, db_get_column_name(Column)) != 0) {
			Ogr_field =
			    OGR_Fld_Create(db_get_column_name(Column),
					   ogr_ftype);
			OGR_L_CreateField(Ogr_layer, Ogr_field, 0);
			OGR_Fld_Destroy(Ogr_field);
		    }
		}
	    }
	    if (keycol == -1)
		G_fatal_error(_("Key column '%s' not found"), Fi->key);
	}
    }

    Ogr_featuredefn = OGR_L_GetLayerDefn(Ogr_layer);

    fout = fskip = nocat = noatt = nocatskip = 0;

    if (OGR_L_TestCapability(Ogr_layer, OLCTransactions))
	OGR_L_StartTransaction(Ogr_layer);

    /* Lines (run always to count features of different type) */
    if ((otype & GV_POINTS) || (otype & GV_LINES)) {
	G_message(_("Exporting %i geometries..."), Vect_get_num_lines(&In));
	for (i = 1; i <= Vect_get_num_lines(&In); i++) {

	    G_percent(i, Vect_get_num_lines(&In), 1);

	    type = Vect_read_line(&In, Points, Cats, i);
	    G_debug(2, "line = %d type = %d", i, type);
	    if (!(otype & type)) {
		G_debug(2, "type %d not specified -> skipping", type);
		fskip++;
		continue;
	    }

	    Vect_cat_get(Cats, field, &cat);
	    if (cat < 0 && !donocat) {	/* Do not export not labeled */
		nocatskip++;
		continue;
	    }

	    /* Geometry */
	    if (type == GV_LINE && poly_flag->answer) {
		OGRGeometryH ring;

		ring = OGR_G_CreateGeometry(wkbLinearRing);
		Ogr_geometry = OGR_G_CreateGeometry(wkbPolygon);

		/* Area */
		for (j = 0; j < Points->n_points; j++) {
		    OGR_G_AddPoint(ring, Points->x[j], Points->y[j],
				   Points->z[j]);
		}
		if (Points->x[Points->n_points - 1] != Points->x[0] ||
		    Points->y[Points->n_points - 1] != Points->y[0] ||
		    Points->z[Points->n_points - 1] != Points->z[0]) {
		    OGR_G_AddPoint(ring, Points->x[0], Points->y[0],
				   Points->z[0]);
		}

		OGR_G_AddGeometryDirectly(Ogr_geometry, ring);
	    }
	    else if ((type == GV_POINT) ||
		     ((type == GV_CENTROID) && (otype & GV_CENTROID))) {
		Ogr_geometry = OGR_G_CreateGeometry(wkbPoint);
		OGR_G_AddPoint(Ogr_geometry, Points->x[0], Points->y[0],
			       Points->z[0]);
	    }
	    else {		/* GV_LINE or GV_BOUNDARY */

		Ogr_geometry = OGR_G_CreateGeometry(wkbLineString);
		for (j = 0; j < Points->n_points; j++) {
		    OGR_G_AddPoint(Ogr_geometry, Points->x[j], Points->y[j],
				   Points->z[j]);
		}
	    }
	    Ogr_feature = OGR_F_Create(Ogr_featuredefn);

	    OGR_F_SetGeometry(Ogr_feature, Ogr_geometry);

	    /* Output one feature for each category */
	    for (j = -1; j < Cats->n_cats; j++) {
		if (j == -1) {
		    if (cat >= 0)
			continue;	/* cat(s) exists */
		}
		else {
		    if (Cats->field[j] == field)
			cat = Cats->cat[j];
		    else
			continue;
		}

		mk_att(cat, Fi, Driver, ncol, doatt, nocat_flag->answer,
		       Ogr_feature);
		OGR_L_CreateFeature(Ogr_layer, Ogr_feature);
	    }
	    OGR_G_DestroyGeometry(Ogr_geometry);
	    OGR_F_Destroy(Ogr_feature);
	}
    }

    /* Areas (run always to count features of different type) */
    if (otype & GV_AREA) {
	G_message(_("Exporting %i areas (may take some time)..."),
		  Vect_get_num_areas(&In));
	for (i = 1; i <= Vect_get_num_areas(&In); i++) {
	    OGRGeometryH ring;

	    G_percent(i, Vect_get_num_areas(&In), 1);

	    centroid = Vect_get_area_centroid(&In, i);
	    cat = -1;
	    if (centroid > 0) {
		Vect_read_line(&In, NULL, Cats, centroid);
		Vect_cat_get(Cats, field, &cat);
	    }
	    G_debug(3, "area = %d centroid = %d ncats = %d", i, centroid,
		    Cats->n_cats);
	    if (cat < 0 && !donocat) {	/* Do not export not labeled */
		nocatskip++;
		continue;
	    }

	    Vect_get_area_points(&In, i, Points);

	    /* Geometry */
	    Ogr_geometry = OGR_G_CreateGeometry(wkbPolygon);

	    ring = OGR_G_CreateGeometry(wkbLinearRing);

	    /* Area */
	    for (j = 0; j < Points->n_points; j++) {
		OGR_G_AddPoint(ring, Points->x[j], Points->y[j],
			       Points->z[j]);
	    }

	    OGR_G_AddGeometryDirectly(Ogr_geometry, ring);

	    /* Isles */
	    for (k = 0; k < Vect_get_area_num_isles(&In, i); k++) {
		Vect_get_isle_points(&In, Vect_get_area_isle(&In, i, k),
				     Points);

		ring = OGR_G_CreateGeometry(wkbLinearRing);
		for (j = 0; j < Points->n_points; j++) {
		    OGR_G_AddPoint(ring, Points->x[j], Points->y[j],
				   Points->z[j]);
		}
		OGR_G_AddGeometryDirectly(Ogr_geometry, ring);
	    }

	    Ogr_feature = OGR_F_Create(Ogr_featuredefn);
	    OGR_F_SetGeometry(Ogr_feature, Ogr_geometry);

	    /* Output one feature for each category */
	    for (j = -1; j < Cats->n_cats; j++) {
		if (j == -1) {
		    if (cat >= 0)
			continue;	/* cat(s) exists */
		}
		else {
		    if (Cats->field[j] == field)
			cat = Cats->cat[j];
		    else
			continue;
		}

		mk_att(cat, Fi, Driver, ncol, doatt, nocat_flag->answer,
		       Ogr_feature);
		OGR_L_CreateFeature(Ogr_layer, Ogr_feature);
	    }
	    OGR_G_DestroyGeometry(Ogr_geometry);
	    OGR_F_Destroy(Ogr_feature);
	}
    }

    /* Faces (run always to count features of different type)  - Faces are similar to lines */
    if (otype & GV_FACE) {
	G_message(_("Exporting %i faces (may take some time) ..."),
		  Vect_get_num_faces(&In));
	for (i = 1; i <= Vect_get_num_faces(&In); i++) {
	    OGRGeometryH ring;

	    G_percent(i, Vect_get_num_faces(&In), 1);

	    type = Vect_read_line(&In, Points, Cats, i);
	    G_debug(3, "line type = %d", type);

	    cat = -1;
	    Vect_cat_get(Cats, field, &cat);

	    G_debug(3, "face = %d ncats = %d", i, Cats->n_cats);
	    if (cat < 0 && !donocat) {	/* Do not export not labeled */
		nocatskip++;
		continue;
	    }

	    if (type & GV_FACE) {

		Ogr_feature = OGR_F_Create(Ogr_featuredefn);

		/* Geometry */
		Ogr_geometry = OGR_G_CreateGeometry(wkbPolygon25D);
		ring = OGR_G_CreateGeometry(wkbLinearRing);

		/* Face */
		for (j = 0; j < Points->n_points; j++) {
		    OGR_G_AddPoint(ring, Points->x[j], Points->y[j],
				   Points->z[j]);
		}

		OGR_G_AddGeometryDirectly(Ogr_geometry, ring);

		OGR_F_SetGeometry(Ogr_feature, Ogr_geometry);

		/* Output one feature for each category */
		for (j = -1; j < Cats->n_cats; j++) {
		    if (j == -1) {
			if (cat >= 0)
			    continue;	/* cat(s) exists */
		    }
		    else {
			if (Cats->field[j] == field)
			    cat = Cats->cat[j];
			else
			    continue;
		    }

		    mk_att(cat, Fi, Driver, ncol, doatt, nocat_flag->answer,
			   Ogr_feature);
		    OGR_L_CreateFeature(Ogr_layer, Ogr_feature);
		}

		OGR_G_DestroyGeometry(Ogr_geometry);
		OGR_F_Destroy(Ogr_feature);
	    }			/* if type & GV_FACE */
	}			/* for */
    }

    /* Kernels */
    if (otype & GV_KERNEL) {
	G_message(_("Exporting %i kernels..."), Vect_get_num_kernels(&In));
	for (i = 1; i <= Vect_get_num_lines(&In); i++) {

	    G_percent(i, Vect_get_num_lines(&In), 1);

	    type = Vect_read_line(&In, Points, Cats, i);
	    G_debug(2, "line = %d type = %d", i, type);
	    if (!(otype & type)) {
		G_debug(2, "type %d not specified -> skipping", type);
		fskip++;
		continue;
	    }

	    Vect_cat_get(Cats, field, &cat);
	    if (cat < 0 && !donocat) {	/* Do not export not labeled */
		nocatskip++;
		continue;
	    }

	    /* Geometry */
	    if (type == GV_KERNEL) {
		Ogr_geometry = OGR_G_CreateGeometry(wkbPoint);
		OGR_G_AddPoint(Ogr_geometry, Points->x[0], Points->y[0],
			       Points->z[0]);

		Ogr_feature = OGR_F_Create(Ogr_featuredefn);

		OGR_F_SetGeometry(Ogr_feature, Ogr_geometry);

		/* Output one feature for each category */
		for (j = -1; j < Cats->n_cats; j++) {
		    if (j == -1) {
			if (cat >= 0)
			    continue;	/* cat(s) exists */
		    }
		    else {
			if (Cats->field[j] == field)
			    cat = Cats->cat[j];
			else
			    continue;
		    }

		    mk_att(cat, Fi, Driver, ncol, doatt, nocat_flag->answer,
			   Ogr_feature);
		    OGR_L_CreateFeature(Ogr_layer, Ogr_feature);
		}
		OGR_G_DestroyGeometry(Ogr_geometry);
		OGR_F_Destroy(Ogr_feature);
	    }
	}
    }

    /*
       TODO:   Volumes. Do not export kernels here, that's already done.
       We do need to worry about holes, though.
       NOTE: We can probably just merge this with faces export function.
       Except for GRASS, which output format would know the difference?
     */
    if ((otype & GV_VOLUME)) {
	G_message(_("Exporting %i volumes..."), Vect_get_num_volumes(&In));
	G_warning(_("Export of volumes not implemented yet. Skipping."));
    }

    if (OGR_L_TestCapability(Ogr_layer, OLCTransactions))
	OGR_L_CommitTransaction(Ogr_layer);

    OGR_DS_Destroy(Ogr_ds);

    Vect_close(&In);

    if (doatt) {
	db_close_database(Driver);
	db_shutdown_driver(Driver);
    }

    /* Summary */
    if (nocat > 0)
	G_warning(_("%d features without category were written"), nocat);
    if (noatt > 0)
	G_warning(_("%d features without attributes were written"), noatt);
    if (nocatskip > 0)
	G_warning(_("%d features found without category were skipped"),
		  nocatskip);

    /* Enable this? May be confusing that for area type are not reported
     *    all boundaries/centroids.
     *  OTOH why should be reported? */
    /*
       if (((otype & GV_POINTS) || (otype & GV_LINES)) && fskip > 0)
       G_warning ("%d features of different type skip", fskip);
     */

    G_done_msg(_("%d features written to <%s> (%s)."), fout,
	       layer_opt->answer, frmt_opt->answer);

    exit(EXIT_SUCCESS);
}


int mk_att(int cat, struct field_info *Fi, dbDriver * Driver, int ncol,
	   int doatt, int nocat, OGRFeatureH Ogr_feature)
{
    int j, ogrfieldnum;
    int colsqltype, colctype, more;
    dbTable *Table;
    dbString dbstring;
    dbColumn *Column;
    dbValue *Value;
    dbCursor cursor;
    char buf[SQL_BUFFER_SIZE];

    G_debug(2, "mk_att() cat = %d, doatt = %d", cat, doatt);
    db_init_string(&dbstring);

    /* Attributes */
    /* Reset */
    if (!doatt) {
	ogrfieldnum = OGR_F_GetFieldIndex(Ogr_feature, "cat");
	OGR_F_UnsetField(Ogr_feature, ogrfieldnum);
	/* doatt reset moved into have cat loop as the table needs to be
	   open to know the OGR field ID. Hopefully this has no ill consequences */
    }

    /* Read & set attributes */
    if (cat >= 0) {		/* Line with category */
	if (doatt) {
	    /* Fetch all attribute records for cat <cat> */ 
	    /* opening and closing the cursor is slow, 
	     * but the cursor really needs to be opened for each cat separately */
	    sprintf(buf, "SELECT * FROM %s WHERE %s = %d", Fi->table, Fi->key, cat);
	    G_debug(2, "SQL: %s", buf);
	    db_set_string(&dbstring, buf);
	    if (db_open_select_cursor
			    (Driver, &dbstring, &cursor, DB_SEQUENTIAL) != DB_OK) {
		    G_fatal_error(_("Cannot select attributes for cat = %d"),
		  cat);
	    }

	    if (db_fetch(&cursor, DB_NEXT, &more) != DB_OK)
		G_fatal_error(_("Unable to fetch data from table"));

	    if (!more) {
		/* G_warning ("No database record for cat = %d", cat); */
		/* Set at least key column to category */
		if (!nocat) {
		    ogrfieldnum = OGR_F_GetFieldIndex(Ogr_feature, Fi->key);
		    OGR_F_SetFieldInteger(Ogr_feature, ogrfieldnum, cat);
		    noatt++;
		}
		else {
		    G_fatal_error(_("No database record for cat = %d and export of 'cat' disabled"),
				  cat);
		}
	    }
	    else {
		Table = db_get_cursor_table(&cursor);
		for (j = 0; j < ncol; j++) {
		    Column = db_get_table_column(Table, j);
		    Value = db_get_column_value(Column);
		    db_convert_column_value_to_string(Column, &dbstring);	/* for debug only */
		    G_debug(2, "col %d : val = %s", j,
			    db_get_string(&dbstring));

		    colsqltype = db_get_column_sqltype(Column);
		    colctype = db_sqltype_to_Ctype(colsqltype);
		    G_debug(2, "  colctype = %d", colctype);

		    if (nocat && strcmp(Fi->key, db_get_column_name(Column)) == 0)
			continue;

		    ogrfieldnum = OGR_F_GetFieldIndex(Ogr_feature,
						      db_get_column_name
						      (Column));

		    G_debug(2, "  column = %s -> fieldnum = %d",
			    db_get_column_name(Column), ogrfieldnum);

		    if (ogrfieldnum < 0) {
			G_debug(4, "Could not get OGR field number for column %s",
				                         db_get_column_name(Column));
			continue;
		    }

		    /* Reset */
		    if ((nocat &&
			 strcmp(Fi->key, db_get_column_name(Column)) ==
			  0) == 0) {
			/* if this is 'cat', then execute the following only if the '-s' flag was NOT given */
			OGR_F_UnsetField(Ogr_feature, ogrfieldnum);
		    }

		    /* prevent writing NULL values */
		    if (!db_test_value_isnull(Value)) {
			if ((nocat &&
			     strcmp(Fi->key, db_get_column_name(Column)) ==
			      0) == 0) {
			    /* if this is 'cat', then execute the following only if the '-s' flag was NOT given */
			    switch (colctype) {
			    case DB_C_TYPE_INT:
				OGR_F_SetFieldInteger(Ogr_feature,
						      ogrfieldnum,
						      db_get_value_int
						      (Value));
				break;
			    case DB_C_TYPE_DOUBLE:
				OGR_F_SetFieldDouble(Ogr_feature, ogrfieldnum,
						     db_get_value_double
						     (Value));
				break;
			    case DB_C_TYPE_STRING:
				OGR_F_SetFieldString(Ogr_feature, ogrfieldnum,
						     db_get_value_string
						     (Value));
				break;
			    case DB_C_TYPE_DATETIME:
				db_convert_column_value_to_string(Column,
								  &dbstring);
				OGR_F_SetFieldString(Ogr_feature, ogrfieldnum,
						     db_get_string
						     (&dbstring));
				break;
			    }
			}
		    }
		}
	    }
	    db_close_cursor(&cursor);
	}
	else {			/* Use cat only */
	    ogrfieldnum = OGR_F_GetFieldIndex(Ogr_feature, "cat");
	    OGR_F_SetFieldInteger(Ogr_feature, ogrfieldnum, cat);
	}
    }
    else {
	/* G_warning ("Line without cat of layer %d", field); */
	nocat++;
    }
    fout++;

    db_free_string(&dbstring);

    return 1;
}


/* to print available drivers in help text */
char *OGR_list_write_drivers(void)
{
    int drn, i;
    OGRSFDriverH Ogr_driver;
    char buf[2000];

    /* Open OGR DSN */
    OGRRegisterAll();
    G_debug(2, "driver count = %d", OGRGetDriverCount());
    drn = -1;
    for (i = 0; i < OGRGetDriverCount(); i++) {
	/* only fetch read/write drivers */
	if (OGR_Dr_TestCapability(OGRGetDriver(i), ODrCCreateDataSource)) {
	    Ogr_driver = OGRGetDriver(i);
	    G_debug(2, "driver %d/%d : %s", i, OGRGetDriverCount(),
		    OGR_Dr_GetName(Ogr_driver));
	    /* chg white space to underscore in OGR driver names */
	    sprintf(buf, "%s", OGR_Dr_GetName(Ogr_driver));
	    G_strchg(buf, ' ', '_');
	    strcat(OGRdrivers, buf);
	    if (i < OGRGetDriverCount() - 1)
		strcat(OGRdrivers, ",");
	}
    }
    G_debug(2, "all drivers: %s", OGRdrivers);
    return OGRdrivers;
}
