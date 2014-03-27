#include <stdlib.h>
#include <grass/Vect.h>
#include <grass/gis.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>
#include "local_proto.h"

static int srch(const void *pa, const void *pb);
static int check_cat(const struct line_cats *, const int *, int);

int bin_to_asc(FILE *ascii,
	       FILE *att, struct Map_info *Map, int ver,
	       int format, int dp, char *fs, int region_flag,
	       int field, char* where, char **columns)
{
    int type, ctype, i, cat, line, left, right, found; 
    double *xptr, *yptr, *zptr, x, y;
    static struct line_pnts *Points;
    struct line_cats *Cats, *ACats;
    char *xstring = NULL, *ystring = NULL, *zstring = NULL;
    struct Cell_head window;
    struct ilist *fcats;

    /* where */
    struct field_info *Fi;
    dbDriver *driver;
    dbValue value = {0};
    dbHandle handle;
    int *cats, ncats;
    
    /* get the region */
    G_get_window(&window);

    ncats = 0;
    cats = NULL;
    
    if (where || columns) {
	Fi = Vect_get_field(Map, field);
	if (!Fi) {
	    G_fatal_error(_("Database connection not defined for layer %d"),
			  field);
	}

	driver = db_start_driver(Fi->driver);
	if (!driver)
	    G_fatal_error(_("Unable to start driver <%s>"), Fi->driver);
	
	db_init_handle(&handle);
	db_set_handle(&handle, Fi->database, NULL);
	
	if (db_open_database(driver, &handle) != DB_OK)
	    G_fatal_error(_("Unable to open database <%s> by driver <%s>"),
			  Fi->database, Fi->driver);
	
	/* select cats (sorted array) */
	ncats = db_select_int(driver, Fi->table, Fi->key, where, &cats);
	G_debug(3, "%d categories selected from table <%s>", ncats, Fi->table);

	if (!columns) {
	    db_close_database(driver);
	    db_shutdown_driver(driver);
	}
    }
    
    Points = Vect_new_line_struct();	/* init line_pnts struct */
    Cats = Vect_new_cats_struct();
    ACats = Vect_new_cats_struct();
    fcats = Vect_new_list();

    /* by default, read_next_line will NOT read Dead lines */
    /* but we can override that (in Level I only) by specifying */
    /* the type  -1, which means match all line types */

    Vect_rewind(Map);

    line = 0;
    while (1) {
	if (-1 == (type = Vect_read_next_line(Map, Points, Cats))) {
	    if (columns) {
		db_close_database(driver);
		db_shutdown_driver(driver);
	    }
	    
	    return -1;
	}

	if (type == -2)	{	/* EOF */
	    if (columns) {
		db_close_database(driver);
		db_shutdown_driver(driver);
	    }
	    return 0;
	}

	line++;
	
	if (format == FORMAT_POINT && !(type & GV_POINTS))
	    continue;
	
	found = check_cat(Cats, cats, ncats);
	if (!found && type == GV_BOUNDARY && Vect_level(Map) > 1) {
	    Vect_get_line_areas(Map, line, &left, &right);
	    if (left < 0) 
		left = Vect_get_isle_area(Map, abs(left));
	    if (left > 0) {
		Vect_get_area_cats(Map, left, ACats);
		found = check_cat(ACats, cats, ncats);
	    }
	    if (right < 0) 
		right = Vect_get_isle_area(Map, abs(right));
	    if (!found && right > 0) {
		Vect_get_area_cats(Map, right, ACats);
		found = check_cat(ACats, cats, ncats);
	    }
	}
	
	if (!found)
	    continue;

	if (ver < 5) {
	    Vect_cat_get(Cats, 1, &cat);
	}

	switch (type) {
	case GV_BOUNDARY:
	    if (ver == 5)
		ctype = 'B';
	    else
		ctype = 'A';
	    break;
	case GV_CENTROID:
	    if (ver < 5) {
		if (att != NULL) {
		    if (cat > 0) {
			G_asprintf(&xstring, "%.*f", dp, Points->x[0]);
			G_trim_decimal(xstring);
			G_asprintf(&ystring, "%.*f", dp, Points->y[0]);
			G_trim_decimal(ystring);
			fprintf(att, "A %s %s %d\n", xstring, ystring, cat);
		    }
		}
		continue;
	    }
	    ctype = 'C';
	    break;
	case GV_LINE:
	    ctype = 'L';
	    break;
	case GV_POINT:
	    ctype = 'P';
	    break;
	case GV_FACE:
	    ctype = 'F';
	    break;
	case GV_KERNEL:
	    ctype = 'K';
	    break;
	default:
	    ctype = 'X';
	    G_warning(_("got type %d"), (int)type);
	    break;
	}

	if (format == FORMAT_POINT) {
	    /*fprintf(ascii, "%c", ctype); */

	    if (region_flag) {
		if ((window.east < Points->x[0]) ||
		    (window.west > Points->x[0]))
		    continue;
	    }
	    G_asprintf(&xstring, "%.*f", dp, Points->x[0]);
	    G_trim_decimal(xstring);

	    if (region_flag) {
		if ((window.north < Points->y[0]) ||
		    (window.south > Points->y[0]))
		    continue;
	    }
	    G_asprintf(&ystring, "%.*f", dp, Points->y[0]);
	    G_trim_decimal(ystring);

	    if (Map->head.with_z && ver == 5) {
		if (region_flag) {
		    if ((window.top < Points->z[0]) ||
			(window.bottom > Points->z[0]))
			continue;
		}
		G_asprintf(&zstring, "%.*f", dp, Points->z[0]);
		G_trim_decimal(zstring);
		fprintf(ascii, "%s%s%s%s%s", xstring, fs, ystring, fs,
			zstring);
	    }
	    else {
		fprintf(ascii, "%s%s%s", xstring, fs, ystring);
	    }

	    Vect_field_cat_get(Cats, field, fcats);
	    
	    if (fcats->n_values > 0) {
		if (fcats->n_values > 1) {
		    G_warning(_("Feature has more categories. Only first category (%d) "
				"is exported."), fcats->value[0]);
		}
		fprintf(ascii, "%s%d", fs, fcats->value[0]);
		
		/* print attributes */
		if (columns) {
		    for(i = 0; columns[i]; i++) {
			if (db_select_value(driver, Fi->table, Fi->key, fcats->value[0],
					    columns[i], &value) < 0)
			    G_fatal_error(_("Unable to select record from table <%s> (key %s, column %s)"),
					  Fi->table, Fi->key, columns[i]);
			
			if (db_test_value_isnull(&value)) {
			    fprintf(ascii, "%s", fs);
			}
			else {
			    switch(db_column_Ctype(driver, Fi->table, columns[i]))
			    {
			    case DB_C_TYPE_INT: {
				fprintf(ascii, "%s%d", fs, db_get_value_int(&value));
				break;
			    }
			    case DB_C_TYPE_DOUBLE: {
				fprintf(ascii, "%s%.*f", fs, dp, db_get_value_double(&value));
				break;
			    }
			    case DB_C_TYPE_STRING: {
				fprintf(ascii, "%s%s", fs, db_get_value_string(&value));
				break;
			    }
			    case DB_C_TYPE_DATETIME: {
				break;
			    }
			    case -1:
				G_fatal_error(_("Column <%s> not found in table <%s>"),
					      columns[i], Fi->table);
			    default: G_fatal_error(_("Column <%s>: unsupported data type"),
						   columns[i]);
			    }
			}
		    }
		}
	    }

	    fprintf(ascii, "\n");
	}
	else {
	    /* FORMAT_STANDARD */
	    if (ver == 5 && Cats->n_cats > 0)
		fprintf(ascii, "%c  %d %d\n", ctype, Points->n_points,
			Cats->n_cats);
	    else
		fprintf(ascii, "%c  %d\n", ctype, Points->n_points);

	    xptr = Points->x;
	    yptr = Points->y;
	    zptr = Points->z;

	    while (Points->n_points--) {

		G_asprintf(&xstring, "%.*f", dp, *xptr++);
		G_trim_decimal(xstring);
		G_asprintf(&ystring, "%.*f", dp, *yptr++);
		G_trim_decimal(ystring);

		if (ver == 5) {
		    if (Map->head.with_z) {
			G_asprintf(&zstring, "%.*f", dp, *zptr++);
			G_trim_decimal(zstring);
			fprintf(ascii, " %-12s %-12s %-12s\n", xstring,
				ystring, zstring);
		    }
		    else {
			fprintf(ascii, " %-12s %-12s\n", xstring, ystring);
		    }
		}		/*Version 4 */
		else {
		    fprintf(ascii, " %-12s %-12s\n", ystring, xstring);
		}
	    }

	    if (ver == 5) {
		for (i = 0; i < Cats->n_cats; i++) {
		    fprintf(ascii, " %-5d %-10d\n", Cats->field[i],
			    Cats->cat[i]);
		}
	    }
	    else {
		if (cat > 0) {
		    if (type == GV_POINT) {
			G_asprintf(&xstring, "%.*f", dp, Points->x[0]);
			G_trim_decimal(xstring);
			G_asprintf(&ystring, "%.*f", dp, Points->y[0]);
			G_trim_decimal(ystring);
			fprintf(att, "P %s %s %d\n", xstring, ystring, cat);
		    }
		    else {
			x = (Points->x[1] + Points->x[0]) / 2;
			y = (Points->y[1] + Points->y[0]) / 2;

			G_asprintf(&xstring, "%.*f", dp, x);
			G_trim_decimal(xstring);
			G_asprintf(&ystring, "%.*f", dp, y);
			G_trim_decimal(ystring);
			fprintf(att, "L %s %s %d\n", xstring, ystring, cat);
		    }
		}
	    }
	}
    }
    /* not reached */
}

int srch(const void *pa, const void *pb)
{
    int *p1 = (int *)pa;
    int *p2 = (int *)pb;
    
    if (*p1 < *p2)
	return -1;
    if (*p1 > *p2)
	return 1;
    return 0;
}

/* check category */
int check_cat(const struct line_cats *Cats, const int *cats, int ncats)
{
    int i;
    
    if (cats) {
	for (i = 0; i < Cats->n_cats; i++) {
	    if ((int *)bsearch((void *) &(Cats->cat[i]), cats, ncats, sizeof(int),
			       srch)) {
		/* found */
		break;
	    }
	}
	
	if (i == Cats->n_cats)
	    return FALSE;
    }
    
    return TRUE;
}
