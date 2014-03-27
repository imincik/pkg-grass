#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>
#include <grass/config.h>

#include "PolimiFunct.h"

/*----------------------------------------------------------------------------------------*/
void P_zero_dim(struct Reg_dimens *dim)
{
    dim->orlo_h = 0.0;
    dim->orlo_v = 0.0;
    dim->overlap = 0.0;
    dim->latoN = 0.0;
    dim->latoE = 0.0;
    return;
}

/*----------------------------------------------------------------------------------------*/
/*
 --------------------------------------------
 |            Elaboration region            |
 |   ------------------------------------   |
 |   |          General region          |   |
 |   |   ----------------------------   |   |
 |   |   |                          |   |   |
 |   |   |                          |   |   |
 |   |   |                          |   |   |
 |   |   |      Overlap region      |   |   |
 |   |   |                          |   |   |
 |   |   |                          |   |   |
 |   |   |                          |   |   |
 |   |   ----------------------------   |   |
 |   |                                  |   |
 |   ------------------------------------   |
 |                                          |
 --------------------------------------------

 The terminology is misleading:
 The Overlap region does NOT overlap with neighbouring segments,
 but the Elaboration and General region do overlap

 Elaboration is used for interpolation
 Interpolated points in Elaboration but outside General are discarded
 Interpolated points in General but outside Overlap are weighed by
 their distance to Overlap and summed up
 Interpolated points in Overlap are taken as they are

 The buffer zones Elaboration - General and General - Overlap must be
 large enough to avoid artefacts 
 */

int
P_set_regions(struct Cell_head *Elaboration, BOUND_BOX * General,
	      BOUND_BOX * Overlap, struct Reg_dimens dim, int type)
{
    /* Set the Elaboration, General, and Overlap region limits
     * Returns 0 on success; -1 on failure*/
    struct Cell_head orig;

    G_get_window(&orig);

    switch (type) {
    case GENERAL_ROW:		/* General case N-S direction */
	Elaboration->north =
	    Elaboration->south + dim.overlap + (2 * dim.orlo_h);
	Elaboration->south = Elaboration->north - dim.latoN;
	General->N = Elaboration->north - dim.orlo_h;
	General->S = Elaboration->south + dim.orlo_h;
	Overlap->N = General->N - dim.overlap;
	Overlap->S = General->S + dim.overlap;
	return 0;

    case GENERAL_COLUMN:	/* General case E-W direction */
	Elaboration->west =
	    Elaboration->east - dim.overlap - (2 * dim.orlo_v);
	Elaboration->east = Elaboration->west + dim.latoE;
	General->W = Elaboration->west + dim.orlo_v;
	General->E = Elaboration->east - dim.orlo_v;
	Overlap->W = General->W + dim.overlap;
	Overlap->E = General->E - dim.overlap;
	return 0;

    case FIRST_ROW:		/* Just started with first row */
	Elaboration->north = orig.north + 2 * dim.orlo_h;
	Elaboration->south = Elaboration->north - dim.latoN;
	General->N = Elaboration->north - 2 * dim.orlo_h;
	General->S = Elaboration->south + dim.orlo_h;
	Overlap->N = General->N;
	Overlap->S = General->S + dim.overlap;
	return 0;

    case LAST_ROW:		/* Reached last row */
	Elaboration->south = orig.south - 2 * dim.orlo_h;
	General->S = Elaboration->south + 2 * dim.orlo_h;
	Overlap->S = General->S;
	return 0;

    case FIRST_COLUMN:		/* Just started with first column */
	Elaboration->west = orig.west - 2 * dim.orlo_v;
	Elaboration->east = Elaboration->west + dim.latoE;
	General->W = Elaboration->west + 2 * dim.orlo_v;
	General->E = Elaboration->east - dim.orlo_v;
	Overlap->W = General->W;
	Overlap->E = General->E - dim.overlap;
	return 0;

    case LAST_COLUMN:		/* Reached last column */
	Elaboration->east = orig.east + 2 * dim.orlo_v;
	General->E = Elaboration->east - 2 * dim.orlo_v;
	Overlap->E = General->E;
	return 0;
    }

    return -1;
}

/*----------------------------------------------------------------------------------------*/
int P_set_dim(struct Reg_dimens *dim, double pe, double pn, int *nsplx, int *nsply)
{
    int total_splines, orlo_splines, n_windows, minsplines;
    double E_extension, N_extension, orloE, orloN;
    struct Cell_head orig;
    int ret = 0;

    G_get_window(&orig);

    E_extension = orig.east - orig.west;
    N_extension = orig.north - orig.south;
    dim->latoE = *nsplx * pe;
    dim->latoN = *nsply * pn;
    orloE = dim->latoE - dim->overlap - 2 * dim->orlo_v;
    orloN = dim->latoN - dim->overlap - 2 * dim->orlo_h;

    /* number of moving windows: E_extension / orloE */
    /* remaining steps: total steps - (floor(E_extension / orloE) * E_extension) / passoE */
    /* remaining steps must be larger than orlo_v + overlap + half of overlap window */
    total_splines = ceil(E_extension / pe);
    orlo_splines = orloE / pe;
    n_windows = floor(E_extension / orloE); /* without last one */
    if (n_windows > 0) {
	minsplines = ceil((double)(dim->latoE / 2.0 - dim->orlo_v - dim->overlap) / pe);
	while (total_splines - orlo_splines * n_windows < minsplines) {
	    *nsplx -= 1;
	    dim->latoE = *nsplx * pe;
	    orloE = dim->latoE - dim->overlap - 2 * dim->orlo_v;

	    orlo_splines = orloE / pe;
	    n_windows = floor(E_extension / orloE); /* without last one */
	    minsplines = ceil((double)(dim->latoE / 2.0 - dim->orlo_v - dim->overlap) / pe);
	    if (ret == 0)
		ret = 1;
	}
	while (total_splines - orlo_splines * n_windows < minsplines * 2 && minsplines > 30) {
	    *nsplx -= 1;
	    dim->latoE = *nsplx * pe;
	    orloE = dim->latoE - dim->overlap - 2 * dim->orlo_v;

	    orlo_splines = orloE / pe;
	    n_windows = floor(E_extension / orloE); /* without last one */
	    minsplines = ceil((double)(dim->latoE / 2.0 - dim->orlo_v - dim->overlap) / pe);
	    if (ret == 0)
		ret = 1;
	}
    }

    total_splines = ceil(N_extension / pn);
    orlo_splines = orloN / pn;
    n_windows = floor(N_extension / orloN); /* without last one */
    if (n_windows > 0) {
	minsplines = ceil((double)(dim->latoN / 2.0 - dim->orlo_h - dim->overlap) / pn);
	while (total_splines - orlo_splines * n_windows < minsplines) {
	    *nsply -= 1;
	    dim->latoN = *nsply * pn;
	    orloN = dim->latoN - dim->overlap - 2 * dim->orlo_h;

	    orlo_splines = orloN / pn;
	    n_windows = floor(N_extension / orloN); /* without last one */
	    minsplines = ceil((double)(dim->latoN / 2.0 - dim->orlo_h - dim->overlap) / pn);
	    if (ret < 2)
		ret += 2;
	}
	while (total_splines - orlo_splines * n_windows < minsplines * 2 && minsplines > 30) {
	    *nsply -= 1;
	    dim->latoN = *nsply * pn;
	    orloN = dim->latoN - dim->overlap - 2 * dim->orlo_h;

	    orlo_splines = orloN / pn;
	    n_windows = floor(N_extension / orloN); /* without last one */
	    minsplines = ceil((double)(dim->latoN / 2.0 - dim->orlo_h - dim->overlap) / pn);
	    if (ret < 2)
		ret += 2;
	}
    }
    return 0;
}

/*----------------------------------------------------------------------------------------*/
int P_get_orlo(int interpolator, struct Reg_dimens *dim, double pe, double pn)
{
    /* Set the orlo regions dimension
     * Returns 1 on success of bilinear; 2 on success of bicubic, 0 on failure */
    if (interpolator == P_BILINEAR) {
       	/* in case of edge artefacts, increase as multiples of 3 */
	dim->orlo_v = 9 * pe;
	dim->orlo_h = 9 * pn;
	return 1;
    }
    else if (interpolator == P_BICUBIC) {
       	/* in case of edge artefacts, increase as multiples of 4 */
	dim->orlo_v = 12 * pe;	/*3 */
	dim->orlo_h = 12 * pn;
	return 2;
    }
    else
	return 0;		/* The interpolator is neither bilinear nor bicubic!! */
}

/*----------------------------------------------------------------------------------------*/
int P_get_BandWidth(int interpolator, int nsplines)
{
    /* Returns the interpolation matrixes BandWidth dimension */

    if (interpolator == P_BILINEAR) {
	return (2 * nsplines + 1);
    }
    else {
	return (4 * nsplines + 3);
    }
}

/*----------------------------------------------------------------------------------------*/
double
P_Mean_Calc(struct Cell_head *Elaboration, struct Point *obs, int npoints)
{
    int i, mean_count = 0;
    double mean = 0.0;
    BOUND_BOX mean_box;

    Vect_region_box(Elaboration, &mean_box);
    mean_box.W -= CONTOUR;
    mean_box.E += CONTOUR;
    mean_box.N += CONTOUR;
    mean_box.S -= CONTOUR;

    for (i = 0; i < npoints; i++) {	/*  */
	if (Vect_point_in_box
	    (obs[i].coordX, obs[i].coordY, obs[i].coordZ, &mean_box)) {
	    mean_count++;
	    mean += obs[i].coordZ;
	}
    }
    if (mean_count == 0)
	mean = .0;
    else
	mean /= (double)mean_count;

    return mean;
}

double P_estimate_splinestep(struct Map_info *Map, double *dens, double *dist)
{
    int type, npoints = 0;
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    double x, y, z;
    struct line_pnts *points;
    struct line_cats *categories;
    BOUND_BOX region_box;
    struct Cell_head orig;

    G_get_set_window(&orig);
    Vect_region_box(&orig, &region_box);

    points = Vect_new_line_struct();
    categories = Vect_new_cats_struct();

    Vect_rewind(Map);
    while ((type = Vect_read_next_line(Map, points, categories)) > 0) {
	if (!(type & GV_POINT))
	    continue;

	x = points->x[0];
	y = points->y[0];
	if (points->z != NULL)
	    z = points->z[0];
	else
	    z = 0.0;

	/* only use points in current region */
	if (Vect_point_in_box(x, y, z, &region_box)) {
	    npoints++;

	    if (npoints > 1) {
		if (xmin > x)
		    xmin = x;
		else if (xmax < x)
		    xmax = x;
		if (ymin > y)
		    ymin = y;
		else if (ymax < y)
		    ymax = y;
	    }
	    else {
		xmin = xmax = x;
		ymin = ymax = y;
	    }
	}
    }
    if (npoints > 0) {
	/* estimated average distance between points in map units */
	*dist = sqrt(((xmax - xmin) * (ymax - ymin)) / npoints);
	/* estimated point density as number of points per square map unit */
	*dens = npoints / ((xmax - xmin) * (ymax - ymin));
	return 0;
    }
    else {
	return -1;
    }
}

struct Point *P_Read_Vector_Region_Map(struct Map_info *Map,
				       struct Cell_head *Elaboration,
				       int *num_points, int dim_vect,
				       int layer)
{
    int line_num, pippo, npoints, cat, type;
    double x, y, z;
    struct Point *obs;
    struct line_pnts *points;
    struct line_cats *categories;
    BOUND_BOX elaboration_box;

    pippo = dim_vect;
    obs = (struct Point *)G_calloc(pippo, sizeof(struct Point));

    points = Vect_new_line_struct();
    categories = Vect_new_cats_struct();

    /* Reading points inside elaboration zone */
    Vect_region_box(Elaboration, &elaboration_box);

    npoints = 0;
    line_num = 0;

    Vect_rewind(Map);
    while ((type = Vect_read_next_line(Map, points, categories)) > 0) {

	if (!(type & GV_POINT))
	    continue;

	line_num++;

	x = points->x[0];
	y = points->y[0];
	if (points->z != NULL)
	    z = points->z[0];
	else
	    z = 0.0;

	/* Reading and storing points only if in elaboration_reg */
	if (Vect_point_in_box(x, y, z, &elaboration_box)) {
	    npoints++;
	    if (npoints >= pippo) {
		pippo += dim_vect;
		obs =
		    (struct Point *)G_realloc((void *)obs,
					      (signed int)pippo *
					      sizeof(struct Point));
	    }

	    /* Storing observation vector */
	    obs[npoints - 1].coordX = x;
	    obs[npoints - 1].coordY = y;
	    obs[npoints - 1].coordZ = z;
	    obs[npoints - 1].lineID = line_num;	/* Storing also the line's number */

	    Vect_cat_get(categories, layer, &cat);
	    obs[npoints - 1].cat = cat;
	}
    }
    Vect_destroy_line_struct(points);
    Vect_destroy_cats_struct(categories);

    *num_points = npoints;
    return obs;
}

/*------------------------------------------------------------------------------------------------*/
int P_Create_Aux2_Table(dbDriver * driver, char *tab_name)
{
    dbTable *auxiliar_tab;
    dbColumn *column;

    auxiliar_tab = db_alloc_table(2);
    db_set_table_name(auxiliar_tab, tab_name);
    db_set_table_description(auxiliar_tab,
			     "Intermediate interpolated values");

    column = db_get_table_column(auxiliar_tab, 0);
    db_set_column_name(column, "ID");
    db_set_column_sqltype(column, DB_SQL_TYPE_INTEGER);

    column = db_get_table_column(auxiliar_tab, 1);
    db_set_column_name(column, "Interp");
    db_set_column_sqltype(column, DB_SQL_TYPE_REAL);

    if (db_create_table(driver, auxiliar_tab) == DB_OK) {
	G_debug(1, _("<%s> created in database."), tab_name);
	return TRUE;
    }
    else
	G_warning(_("<%s> has not been created in database."), tab_name);

    return FALSE;
}

/*------------------------------------------------------------------------------------------------*/
int P_Create_Aux4_Table(dbDriver * driver, char *tab_name)
{
    dbTable *auxiliar_tab;
    dbColumn *column;

    auxiliar_tab = db_alloc_table(4);
    db_set_table_name(auxiliar_tab, tab_name);
    db_set_table_description(auxiliar_tab,
			     "Intermediate interpolated values");

    column = db_get_table_column(auxiliar_tab, 0);
    db_set_column_name(column, "ID");
    db_set_column_sqltype(column, DB_SQL_TYPE_INTEGER);

    column = db_get_table_column(auxiliar_tab, 1);
    db_set_column_name(column, "Interp");
    db_set_column_sqltype(column, DB_SQL_TYPE_REAL);

    column = db_get_table_column(auxiliar_tab, 2);
    db_set_column_name(column, "X");
    db_set_column_sqltype(column, DB_SQL_TYPE_DOUBLE_PRECISION);

    column = db_get_table_column(auxiliar_tab, 3);
    db_set_column_name(column, "Y");
    db_set_column_sqltype(column, DB_SQL_TYPE_DOUBLE_PRECISION);

    if (db_create_table(driver, auxiliar_tab) == DB_OK) {
	G_debug(1, _("<%s> created in database."), tab_name);
	return TRUE;
    }
    else
	G_warning(_("<%s> has not been created in database."), tab_name);

    return FALSE;
}

/*------------------------------------------------------------------------------------------------*/
int P_Drop_Aux_Table(dbDriver * driver, char *tab_name)
{
    dbString drop;

    db_init_string(&drop);
    db_append_string(&drop, "drop table ");
    db_append_string(&drop, tab_name);
    return db_execute_immediate(driver, &drop);
}

/*---------------------------------------------------------------------------------------*/
void P_Aux_to_Raster(double **matrix, int fd)
{
    int ncols, col, nrows, row;
    void *ptr, *raster;
    struct Cell_head Original;

    G_get_window(&Original);
    nrows = G_window_rows();
    ncols = G_window_cols();

    raster = G_allocate_raster_buf(DCELL_TYPE);

    for (row = 0; row < nrows; row++) {
	G_percent(row, nrows, 2);

	G_set_d_null_value(raster, ncols);

	for (col = 0, ptr = raster; col < ncols;
	     col++, ptr = G_incr_void_ptr(ptr, G_raster_size(DCELL_TYPE))) {
	    G_set_raster_value_d(ptr, (DCELL) (matrix[row][col]), DCELL_TYPE);
	}
	G_put_d_raster_row(fd, raster);
    }
    G_percent(row, nrows, 2);
}

/*------------------------------------------------------------------------------------------------*/
void
P_Aux_to_Vector(struct Map_info *Map, struct Map_info *Out, dbDriver * driver,
		char *tab_name)
{

    int more, line_num, type, count = 0;
    double coordX, coordY, coordZ;

    struct line_pnts *point;
    struct line_cats *cat;
    dbTable *table;
    dbColumn *column;
    dbValue *value;
    dbCursor cursor;
    dbString sql;

    char buf[1024];

    point = Vect_new_line_struct();
    cat = Vect_new_cats_struct();

    db_init_string(&sql);
    db_zero_string(&sql);

    sprintf(buf, "select ID, X, Y, sum(Interp) from %s group by ID, X, Y",
	    tab_name);

    db_append_string(&sql, buf);
    db_open_select_cursor(driver, &sql, &cursor, DB_SEQUENTIAL);

    while (db_fetch(&cursor, DB_NEXT, &more) == DB_OK && more) {
	count++;
	table = db_get_cursor_table(&cursor);

	column = db_get_table_column(table, 0);
	type = db_sqltype_to_Ctype(db_get_column_sqltype(column));
	if (type == DB_C_TYPE_INT)
	    value = db_get_column_value(column);
	else
	    continue;
	line_num = db_get_value_int(value);

	column = db_get_table_column(table, 1);
	type = db_sqltype_to_Ctype(db_get_column_sqltype(column));
	if (type == DB_C_TYPE_DOUBLE)
	    value = db_get_column_value(column);
	else
	    continue;
	coordZ = db_get_value_double(value);

	column = db_get_table_column(table, 2);
	type = db_sqltype_to_Ctype(db_get_column_sqltype(column));
	if (type == DB_C_TYPE_DOUBLE)
	    value = db_get_column_value(column);
	else
	    continue;
	coordX = db_get_value_double(value);

	column = db_get_table_column(table, 3);
	type = db_sqltype_to_Ctype(db_get_column_sqltype(column));
	if (type == DB_C_TYPE_DOUBLE)
	    value = db_get_column_value(column);
	else
	    continue;
	coordY = db_get_value_double(value);

	Vect_copy_xyz_to_pnts(point, &coordX, &coordY, &coordZ, 1);
	Vect_reset_cats(cat);
	Vect_cat_set(cat, 1, 1);
	Vect_write_line(Out, GV_POINT, point, cat);
    }
    return;
}

/*------------------------------------------------------------------------------------------------*/
#ifdef notdef
double **P_Null_Matrix(double **matrix)
{
    int nrows, row, ncols, col;
    struct Cell_head Original;


    G_get_window(&Original);
    nrows = G_window_rows();
    ncols = G_window_cols();

    for (row = 0; row < nrows; row++) {
	for (col = 0; col < ncols; col++) {
	    matrix[row][col] = NULL;
	}
    }
}
#endif

/*! DEFINITION OF THE SUBZONES

  5: inside Overlap region
  all others: inside General region but outside Overlap region

   ---------------------------------
   | |       | |       | |       | |
   ---------------------------------
   | |       | |       | |       | |
   | |       | |       | |       | |
   | |       | |       | |       | |
   ---------------------------------
   | |       |4|   3   |3|       | |
   ---------------------------------
   | |       | |       | |       | |
   | |       |2|   5   |1|       | |
   | |       | |       | |       | |
   ---------------------------------
   | |       |2|   1   |1|       | |
   ---------------------------------
   | |       | |       | |       | |
   | |       | |       | |       | |
   | |       | |       | |       | |
   ---------------------------------
   | |       | |       | |       | |
   ---------------------------------
 */
