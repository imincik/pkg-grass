
/****************************************************************************
 *
 * MODULE:       r.to.vect
 *
 * AUTHOR(S):    Bill Brown, Mike Baba, Jean Ezell and Andrew Heekin,
 *               David Satnik, Andrea Aime, Radim Blazek
 *
 * PURPOSE:      Converts a raster map into a vector map layer
 *
 * COPYRIGHT:    (C) 2007 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#define MAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/dbmi.h>
#include <grass/Vect.h>
#include <grass/glocale.h>
#include "global.h"

/* 
 * Attributes for lines are ignored. For points and area by default unique new
 * category is assigned to each and raster value is written to 'value' column.
 * Labels are written to 'label' column if exists. If value flag (-v) is used
 * and type is CELL, raster values are used as categories. 
 *
 * 2007/2: attributes for lines supported
 */

int main(int argc, char *argv[])
{
    struct GModule *module;
    struct Option *in_opt, *out_opt, *feature_opt;
    struct Flag *smooth_flg, *value_flg, *z_flg, *quiet, *no_topol;
    char *mapset;
    int feature;


    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, conversion, vectorization");
    module->description = _("Converts a raster map into a vector map layer.");

    in_opt = G_define_standard_option(G_OPT_R_INPUT);

    out_opt = G_define_standard_option(G_OPT_V_OUTPUT);

    feature_opt = G_define_option();
    feature_opt->key = "feature";
    feature_opt->type = TYPE_STRING;
    feature_opt->required = YES;
    feature_opt->multiple = NO;
    feature_opt->options = "point,line,area";
    feature_opt->answer = "line";
    feature_opt->description = _("Feature type");

    smooth_flg = G_define_flag();
    smooth_flg->key = 's';
    smooth_flg->description = _("Smooth corners of area features");

    value_flg = G_define_flag();
    value_flg->key = 'v';
    value_flg->description =
	_("Use raster values as categories instead of unique sequence (CELL only)");
    value_flg->guisection = _("Attributes");

    z_flg = G_define_flag();
    z_flg->key = 'z';
    z_flg->description =
	_("Write raster values as z coordinate. Table is not created. "
	  "Currently supported only for points.");
    z_flg->guisection = _("Attributes");

    no_topol = G_define_flag();
    no_topol->key = 'b';
    no_topol->description =
	_("Do not build vector topology (use with care for massive point export)");

    /* please, remove before GRASS 7 released */
    quiet = G_define_flag();
    quiet->key = 'q';
    quiet->description = _("Quiet - Do not show progress");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* please, remove before GRASS 7 released */
    if (quiet->answer) {
	putenv("GRASS_VERBOSE=0");
	G_warning(_("The '-q' flag is superseded and will be removed "
		    "in future. Please use '--quiet' instead."));
    }


    feature = Vect_option_to_types(feature_opt);
    smooth_flag = (smooth_flg->answer) ? SMOOTH : NO_SMOOTH;
    value_flag = value_flg->answer;

    if (z_flg->answer && (feature != GV_POINT))
	G_fatal_error(_("z flag is supported only for points"));

    /* Open files */
    if ((mapset = G_find_cell(in_opt->answer, "")) == NULL)
	G_fatal_error(_("Raster map <%s> not found"), in_opt->answer);

    if ((input_fd = G_open_cell_old(in_opt->answer, mapset)) < 0)
	G_fatal_error(_("Unable to open raster map <%s>"), in_opt->answer);

    data_type = G_get_raster_map_type(input_fd);
    data_size = G_raster_size(data_type);
    G_get_window(&cell_head);

    if (value_flag && data_type != CELL_TYPE) {
	G_warning(_("Raster is not CELL, '-v' flag ignored, raster values will be written to the table."));
	value_flag = 0;
    }

    if (z_flg->answer)
	Vect_open_new(&Map, out_opt->answer, 1);
    else
	Vect_open_new(&Map, out_opt->answer, 0);

    Vect_hist_command(&Map);

    Cats = Vect_new_cats_struct();

    /* Open category labels */
    if (data_type == CELL_TYPE) {
	if (0 == G_read_cats(in_opt->answer, mapset, &RastCats))
	    has_cats = 1;
    }
    else
	has_cats = 0;

    db_init_string(&sql);
    db_init_string(&label);

    /* Create table */
    if ((feature & (GV_AREA | GV_POINT | GV_LINE)) &&
	(!value_flag || (value_flag && has_cats)) && !(z_flg->answer)) {
	char buf[1000];

	Fi = Vect_default_field_info(&Map, 1, NULL, GV_1TABLE);
	Vect_map_add_dblink(&Map, 1, NULL, Fi->table, "cat", Fi->database,
			    Fi->driver);

	driver = db_start_driver_open_database(Fi->driver, Fi->database);
	if (driver == NULL)
	    G_fatal_error(_("Unable to open database <%s> by driver <%s>"),
			  Fi->database, Fi->driver);

	/* Create new table */
	db_zero_string(&sql);
	sprintf(buf, "create table %s ( cat integer", Fi->table);
	db_append_string(&sql, buf);

	if (!value_flag) {	/* add value to the table */
	    if (data_type == CELL_TYPE)
		db_append_string(&sql, ", value integer");
	    else
		db_append_string(&sql, ", value double precision");
	}

	if (has_cats) {
	    int i, len;
	    int clen = 0;

	    /* Get maximum column length */
	    for (i = 0; i < RastCats.ncats; i++) {
		len = strlen(RastCats.labels[i]);
		if (len > clen)
		    clen = len;
	    }
	    clen += 10;

	    sprintf(buf, ", label varchar(%d)", clen);
	    db_append_string(&sql, buf);
	}

	db_append_string(&sql, ")");

	G_debug(3, db_get_string(&sql));

	if (db_execute_immediate(driver, &sql) != DB_OK)
	    G_fatal_error(_("Unable to create table: %s"),
			  db_get_string(&sql));

	if (db_create_index2(driver, Fi->table, "cat") != DB_OK)
	    G_warning(_("Unable to create index"));

	if (db_grant_on_table
	    (driver, Fi->table, DB_PRIV_SELECT,
	     DB_GROUP | DB_PUBLIC) != DB_OK)
	    G_fatal_error(_("Unable to grant privileges on table <%s>"),
			  Fi->table);

	db_begin_transaction(driver);

    }
    else {
	driver = NULL;
    }

    /* init variables for lines and areas */
    first_read = 1;
    last_read = 0;
    direction = FORWARD;
    row_length = cell_head.cols;
    n_rows = cell_head.rows;
    row_count = 0;

    if (feature == GV_LINE) {
	alloc_lines_bufs(row_length + 2);
	extract_lines();
    }
    else if (feature == GV_AREA) {
	alloc_areas_bufs(row_length + 2);
	extract_areas();
    }
    else {			/* GV_POINT */

	extract_points(z_flg->answer);
    }

    G_close_cell(input_fd);

    if (!no_topol->answer)
	Vect_build(&Map);


    /* insert cats and optionally labels if raster cats were used */
    if (driver && value_flag) {
	char buf[1000];
	int c, i, cat, fidx, ncats, lastcat, tp, id;

	fidx = Vect_cidx_get_field_index(&Map, 1);
	if (fidx >= 0) {
	    ncats = Vect_cidx_get_num_cats_by_index(&Map, fidx);
	    lastcat = -1;

	    for (c = 0; c < ncats; c++) {
		Vect_cidx_get_cat_by_index(&Map, fidx, c, &cat, &tp, &id);

		if (lastcat == cat)
		    continue;

		/* find label, slow -> TODO faster */
		db_set_string(&label, "");
		for (i = 0; i < RastCats.ncats; i++) {
		    if (cat == (int)RastCats.q.table[i].dLow) {	/* cats are in dLow/High not in cLow/High !!! */
			db_set_string(&label, RastCats.labels[i]);
			db_double_quote_string(&label);
			break;
		    }
		}
		G_debug(3, "cat = %d label = %s", cat, db_get_string(&label));

		sprintf(buf, "insert into %s values ( %d, '%s')", Fi->table,
			cat, db_get_string(&label));
		db_set_string(&sql, buf);
		G_debug(3, db_get_string(&sql));

		if (db_execute_immediate(driver, &sql) != DB_OK)
		    G_fatal_error(_("Unable to insert into table: %s"),
				  db_get_string(&sql));

		lastcat = cat;
	    }
	}
    }

    if (has_cats)
	G_free_cats(&RastCats);

    if (driver != NULL) {
	db_commit_transaction(driver);
	db_close_database_shutdown_driver(driver);
    }

    Vect_close(&Map);
    G_done_msg("");

    exit(EXIT_SUCCESS);
}
