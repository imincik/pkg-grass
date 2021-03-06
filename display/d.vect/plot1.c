/* plot1() - Level One vector reading */

#include <math.h>
#include <string.h>

#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/display.h>
#include <grass/raster.h>
#include <grass/symbol.h>
#include <grass/glocale.h>
#include <grass/dbmi.h>

#include "plot.h"
#include "local_proto.h"

#define RENDER_POLYLINE 0
#define RENDER_POLYGON  1

int palette_ncolors = 16;

struct rgb_color palette[16] = {
    {198, 198, 198},		/*  1: light gray */
    {127, 127, 127},		/*  2: medium/dark gray */
    {255, 0, 0},		/*  3: bright red */
    {139, 0, 0},		/*  4: dark red */
    {0, 255, 0},		/*  5: bright green */
    {0, 139, 0},		/*  6: dark green */
    {0, 0, 255},		/*  7: bright blue */
    {0, 0, 139},		/*  8: dark blue   */
    {255, 255, 0},		/*  9: yellow */
    {139, 126, 10},		/* 10: olivey brown */
    {255, 165, 0},		/* 11: orange */
    {255, 192, 203},		/* 12: pink   */
    {255, 0, 255},		/* 13: magenta */
    {139, 0, 139},		/* 14: dark magenta */
    {0, 255, 255},		/* 15: cyan */
    {0, 139, 139}		/* 16: dark cyan */
};

/*local functions */
static void local_plot_poly(double *xf, double *yf, int n, int type);

/*global render switch */
int render;

/* *************************************************************** */
/* function to plot polygons and polylines       ***************** */
/* the parameter type switches render mode       ***************** */
/* *************************************************************** */
static void local_plot_poly(double *xf, double *yf, int n, int type)
{
    static int *xi, *yi;
    static int nalloc;
    int i;

    if (nalloc < n) {
	nalloc = n;
	xi = G_realloc(xi, nalloc * sizeof(int));
	yi = G_realloc(yi, nalloc * sizeof(int));
    }

    for (i = 0; i < n; i++) {
	xi[i] = (int)floor(0.5 + D_u_to_d_col(xf[i]));
	yi[i] = (int)floor(0.5 + D_u_to_d_row(yf[i]));
    }

    if (type == RENDER_POLYGON)
	R_polygon_abs(xi, yi, n);
    else
	R_polyline_abs(xi, yi, n);
}

/* *************************************************************** */
/* function to use different render methods for polylines ******** */
/* *************************************************************** */
void plot_polyline(double *xf, double *yf, int n)
{
    int i;

    switch (render) {
    case RENDER_DP:
	D_polyline(xf, yf, n);
	break;
    case RENDER_DPC:
	D_polyline_clip(xf, yf, n);
	break;
    case RENDER_DPL:
	D_polyline_cull(xf, yf, n);
	break;
    case RENDER_RPA:
	local_plot_poly(xf, yf, n, RENDER_POLYLINE);
	break;
    case RENDER_GPP:
    default:
	for (i = 1; i < n; i++)
	    G_plot_line(xf[i - 1], yf[i - 1], xf[i], yf[i]);
	break;
    }
}

/* *************************************************************** */
/* function to use different render methods for polygons  ******** */
/* *************************************************************** */
void plot_polygon(double *xf, double *yf, int n)
{
    switch (render) {
    case RENDER_GPP:
	G_plot_polygon(xf, yf, n);
	break;
    case RENDER_DP:
	D_polygon(xf, yf, n);
	break;
    case RENDER_DPC:
	D_polygon_clip(xf, yf, n);
	break;
    case RENDER_DPL:
	D_polygon_cull(xf, yf, n);
	break;
    case RENDER_RPA:
	local_plot_poly(xf, yf, n, RENDER_POLYGON);
	break;
    default:
	G_plot_polygon(xf, yf, n);
	break;
    }
}

/* *************************************************************** */
/* *************************************************************** */
/* *************************************************************** */
int plot1(struct Map_info *Map, int type, struct cat_list *Clist,
	  const struct color_rgb *color, const struct color_rgb *fcolor,
	  int chcat, char *symbol_name, double size, char *size_column,
	  char *rot_column, int id_flag, int table_colors_flag,
	  int cats_color_flag, char *rgb_column, int default_width,
	  char *width_column, double width_scale, int z_color_flag,
	  char *style)
{
    int i, ltype, nlines = 0, line, cat = -1;
    double *x, *y;
    struct line_pnts *Points, *PPoints;
    struct line_cats *Cats;
    double msize;
    int x0, y0;

    struct field_info *fi = NULL;
    dbDriver *driver = NULL;
    dbCatValArray cvarr_rgb, cvarr_width, cvarr_size, cvarr_rot;
    dbCatVal *cv_rgb = NULL, *cv_width = NULL, *cv_size = NULL, *cv_rot = NULL;
    int nrec_rgb = 0, nrec_width = 0, nrec_size = 0, nrec_rot = 0;
    int nerror_rgb;
    
    int open_db;
    int custom_rgb = FALSE;
    char colorstring[12];	/* RRR:GGG:BBB */
    int red, grn, blu;
    RGBA_Color *line_color, *fill_color, *primary_color;
    unsigned char which;
    int width;
    SYMBOL *Symb = NULL;
    double var_size, rotation;

    var_size = size;
    rotation = 0.0;
    nerror_rgb = 0;

    line_color = G_malloc(sizeof(RGBA_Color));
    fill_color = G_malloc(sizeof(RGBA_Color));
    primary_color = G_malloc(sizeof(RGBA_Color));

    primary_color->a = RGBA_COLOR_OPAQUE;

    /* change function prototype to pass RGBA_Color instead of color_rgb? */
    if (color) {
	line_color->r = color->r;
	line_color->g = color->g;
	line_color->b = color->b;
	line_color->a = RGBA_COLOR_OPAQUE;
    }
    else
	line_color->a = RGBA_COLOR_NONE;

    if (fcolor) {
	fill_color->r = fcolor->r;
	fill_color->g = fcolor->g;
	fill_color->b = fcolor->b;
	fill_color->a = RGBA_COLOR_OPAQUE;
    }
    else
	fill_color->a = RGBA_COLOR_NONE;


    msize = size * (D_d_to_u_col(2.0) - D_d_to_u_col(1.0));	/* do it better */

    Points = Vect_new_line_struct();
    PPoints = Vect_new_line_struct();
    Cats = Vect_new_cats_struct();

    open_db = table_colors_flag || width_column || size_column || rot_column;

    if (open_db) {
	fi = Vect_get_field(Map, (Clist->field > 0 ? Clist->field : 1));
	if (fi == NULL) {
	    G_fatal_error(_("Database connection not defined for layer %d"),
			  (Clist->field > 0 ? Clist->field : 1));
	}

	driver = db_start_driver_open_database(fi->driver, fi->database);
	if (driver == NULL)
	    G_fatal_error(_("Unable to open database <%s> by driver <%s>"),
			  fi->database, fi->driver);
    }

    if (table_colors_flag) {
	/* for reading RRR:GGG:BBB color strings from table */

	if (rgb_column == NULL || *rgb_column == '\0') {
	    if (open_db)
		db_close_database_shutdown_driver(driver);
	    
	    G_fatal_error(_("Color definition column not specified"));
	}

	db_CatValArray_init(&cvarr_rgb);

	nrec_rgb = db_select_CatValArray(driver, fi->table, fi->key,
					 rgb_column, NULL, &cvarr_rgb);

	G_debug(3, "nrec_rgb (%s) = %d", rgb_column, nrec_rgb);

	if (cvarr_rgb.ctype != DB_C_TYPE_STRING)
	    G_warning(_("Color definition column ('%s') not a string. "
			"Column must be of form 'RRR:GGG:BBB' where RGB values range 0-255. "
			"You can use '%s' module to define color rules. "
                       "Unable to colorize features."),
		      rgb_column, "v.colors");
       else {
	   if (nrec_rgb < 0) {
	       if (open_db)
		   db_close_database_shutdown_driver(driver);
	       
	       G_fatal_error(_("Cannot select data (%s) from table"),
			     rgb_column);
	   }
	   G_debug(2, "\n%d records selected from table", nrec_rgb);

	   /*
	   for (i = 0; i < cvarr_rgb.n_values; i++) {
	       G_debug(4, "cat = %d  %s = %s", cvarr_rgb.value[i].cat,
		       rgb_column, db_get_string(cvarr_rgb.value[i].val.s));
	   }
	   */
       }
    }
    if (width_column) {
	if (*width_column == '\0') {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Line width column not specified."));
	}

	db_CatValArray_init(&cvarr_width);

	nrec_width = db_select_CatValArray(driver, fi->table, fi->key,
					   width_column, NULL, &cvarr_width);

	G_debug(3, "nrec_width (%s) = %d", width_column, nrec_width);

	if (cvarr_width.ctype != DB_C_TYPE_INT &&
	    cvarr_width.ctype != DB_C_TYPE_DOUBLE) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Line width column (%s) is not numeric."),
			  width_column);
	}
	
	if (nrec_width < 0) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Cannot select data (%s) from table"),
			  width_column);
	}
	
	G_debug(2, " %d records selected from table", nrec_width);

	for (i = 0; i < cvarr_width.n_values; i++) {
	    G_debug(4, "(width) cat = %d  %s = %d", cvarr_width.value[i].cat,
		    width_column,
		    (cvarr_width.ctype ==
		     DB_C_TYPE_INT ? cvarr_width.value[i].val.
		     i : (int)cvarr_width.value[i].val.d));
	}
    }

    if (size_column) {
	if (*size_column == '\0') {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Symbol size column not specified."));
	}
	
	db_CatValArray_init(&cvarr_size);

	nrec_size = db_select_CatValArray(driver, fi->table, fi->key,
					   size_column, NULL, &cvarr_size);

	G_debug(3, "nrec_size (%s) = %d", size_column, nrec_size);

	if (cvarr_size.ctype != DB_C_TYPE_INT &&
	    cvarr_size.ctype != DB_C_TYPE_DOUBLE) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Symbol size column (%s) is not numeric."),
			  size_column);
	}
	
	if (nrec_size < 0) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Cannot select data (%s) from table"),
			  size_column);
	}
	
	G_debug(2, " %d records selected from table", nrec_size);

	for (i = 0; i < cvarr_size.n_values; i++) {
	    G_debug(4, "(size) cat = %d  %s = %d", cvarr_size.value[i].cat,
		    size_column,
		    (cvarr_size.ctype ==
		     DB_C_TYPE_INT ? cvarr_size.value[i].val.
		     i : (int)cvarr_size.value[i].val.d));
	}
    }

    if (rot_column) {
	if (*rot_column == '\0') {
	    if (open_db)
		db_close_database_shutdown_driver(driver);

	    G_fatal_error(_("Symbol rotation column not specified."));
	}
	
	db_CatValArray_init(&cvarr_rot);

	nrec_rot = db_select_CatValArray(driver, fi->table, fi->key,
					   rot_column, NULL, &cvarr_rot);

	G_debug(3, "nrec_rot (%s) = %d", rot_column, nrec_rot);

	if (cvarr_rot.ctype != DB_C_TYPE_INT &&
	    cvarr_rot.ctype != DB_C_TYPE_DOUBLE) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);
	    
	    G_fatal_error(_("Symbol rotation column (%s) is not numeric."),
			  rot_column);
	}

 	if (nrec_rot < 0) {
	    if (open_db)
		db_close_database_shutdown_driver(driver);
	    
	    G_fatal_error(_("Cannot select data (%s) from table"),
			  rot_column);
	}
	
	G_debug(2, " %d records selected from table", nrec_rot);

	for (i = 0; i < cvarr_rot.n_values; i++) {
	    G_debug(4, "(rot) cat = %d  %s = %d", cvarr_rot.value[i].cat,
		    rot_column,
		    (cvarr_rot.ctype ==
		     DB_C_TYPE_INT ? cvarr_rot.value[i].val.
		     i : (int)cvarr_rot.value[i].val.d));
	}
    }

    if( !(nrec_size || nrec_rot) ) {
	Symb = S_read(symbol_name);
	if (Symb == NULL)
	    G_warning(_("Unable to read symbol, unable to display points"));
	else
	    S_stroke(Symb, (int)(size + 0.5), 0.0, 0);
    }

    if (open_db)
	db_close_database_shutdown_driver(driver);

    Vect_rewind(Map);

    /* Is it necessary to reset line/label color in each loop ? */

    if (color && !table_colors_flag && !cats_color_flag)
	R_RGB_color(color->r, color->g, color->b);

    if (Vect_level(Map) >= 2)
	nlines = Vect_get_num_lines(Map);

    line = 0;
    while (1) {
	line++;
	if (Vect_level(Map) >= 2) {
	    if (line > nlines)
		break;
	    if (!Vect_line_alive(Map, line))
		continue;
	    ltype = Vect_read_line(Map, Points, Cats, line);
	}
	else {
	    ltype = Vect_read_next_line(Map, Points, Cats);
	    if (ltype == -1) {
		G_fatal_error(_("Unable to read vector map"));
	    }
	    else if (ltype == -2) { /* EOF */
		break;
	    }
	}

	if (!(type & ltype))
	    continue;

	if (Points->n_points == 0)
	    continue;

	if (chcat) {
	    int found = 0;

	    if (id_flag) {	/* use line id */
		if (!(Vect_cat_in_cat_list(line, Clist)))
		    continue;
	    }
	    else {
		for (i = 0; i < Cats->n_cats; i++) {
		    if (Cats->field[i] == Clist->field &&
			Vect_cat_in_cat_list(Cats->cat[i], Clist)) {
			found = 1;
			break;
		    }
		}
		if (!found)
		    continue;
	    }
	}
	else if (Clist->field > 0) {
	    int found = 0;

	    for (i = 0; i < Cats->n_cats; i++) {
		if (Cats->field[i] == Clist->field) {
		    found = 1;
		    break;
		}
	    }
	    /* lines with no category will be displayed */
	    if (Cats->n_cats > 0 && !found)
		continue;
	}

	/* z height colors */
	if (z_color_flag && Vect_is_3d(Map)) {
	    BOUND_BOX box;
	    double zval;
	    struct Colors colors;

	    Vect_get_map_box(Map, &box);
	    zval = Points->z[0];
	    G_debug(3, "display line %d, cat %d, x: %f, y: %f, z: %f", line,
		    cat, Points->x[0], Points->y[0], Points->z[0]);
	    custom_rgb = TRUE;
	    G_make_fp_colors(&colors, style, box.B, box.T);
	    G_get_raster_color(&zval, &red, &grn, &blu, &colors, DCELL_TYPE);
	    G_debug(3, "b %d, g: %d, r %d", blu, grn, red);
	}

	if (table_colors_flag) {
	    /* only first category */
	    Vect_cat_get(Cats, 
			 (Clist->field > 0 ? Clist->field :
			  (Cats->n_cats >
			   0 ? Cats->field[0] : 1)), &cat);

	    if (cat >= 0) {
		G_debug(3, "display element %d, cat %d", line, cat);

		/* Read RGB colors from db for current area # */
		if (db_CatValArray_get_value(&cvarr_rgb, cat, &cv_rgb) !=
		    DB_OK) {
		    custom_rgb = FALSE;
		}
		else {
		    sprintf(colorstring, "%s", db_get_string(cv_rgb->val.s));

		    if (*colorstring != '\0') {
			G_debug(3, "element %d: colorstring: %s", line,
				colorstring);

			if (G_str_to_color(colorstring, &red, &grn, &blu) ==
			    1) {
			    custom_rgb = TRUE;
			    G_debug(3, "element:%d  cat %d r:%d g:%d b:%d",
				    line, cat, red, grn, blu);
			}
			else {
			    custom_rgb = FALSE;
			    G_important_message(_("Error in color definition column '%s', feature id %d "
						  "with cat %d: colorstring '%s'"),
						rgb_column, line, cat, colorstring);
			    nerror_rgb++;
			}
		    }
		    else {
			custom_rgb = FALSE;
			G_important_message(_("Error in color definition column '%s', feature id %d "
					      "with cat %d"),
					    rgb_column, line, cat);
			nerror_rgb++;
		    }
		}
	    }			/* end if cat */
	    else {
		custom_rgb = FALSE;
	    }
	}			/* end if table_colors_flag */


	/* random colors */
	if (cats_color_flag) {
	    custom_rgb = FALSE;
	    if (Clist->field > 0) {
		Vect_cat_get(Cats, Clist->field, &cat);
		if (cat >= 0) {
		    G_debug(3, "display element %d, cat %d", line, cat);
		    /* fetch color number from category */
		    which = (cat % palette_ncolors);
		    G_debug(3, "cat:%d which color:%d r:%d g:%d b:%d", cat,
			    which, palette[which].R, palette[which].G,
			    palette[which].B);

		    custom_rgb = TRUE;
		    red = palette[which].R;
		    grn = palette[which].G;
		    blu = palette[which].B;
		}
	    }
	    else if (Cats->n_cats > 0) {
		/* fetch color number from layer */
		which = (Cats->field[0] % palette_ncolors);
		G_debug(3, "layer:%d which color:%d r:%d g:%d b:%d",
			Cats->field[0], which, palette[which].R,
			palette[which].G, palette[which].B);

		custom_rgb = TRUE;
		red = palette[which].R;
		grn = palette[which].G;
		blu = palette[which].B;
	    }
	}


	if (nrec_width) {
	    /* only first category */
	    Vect_cat_get(Cats,
			 (Clist->field > 0 ? Clist->field :
			  (Cats->n_cats >
			   0 ? Cats->field[0] : 1)), &cat);

	    if (cat >= 0) {
		G_debug(3, "display element %d, cat %d", line, cat);

		/* Read line width from db for current area # */
		if (db_CatValArray_get_value(&cvarr_width, cat, &cv_width) !=
		    DB_OK) {
		    width = default_width;
		}
		else {
		    width =
			width_scale * (cvarr_width.ctype ==
				       DB_C_TYPE_INT ? cv_width->val.
				       i : (int)cv_width->val.d);
		    if (width < 0) {
			G_warning(_("Error in line width column (%s), element %d "
				   "with cat %d: line width [%d]"),
				  width_column, line, cat, width);
			width = default_width;
		    }
		}
	    }		/* end if cat */
	    else {
		width = default_width;
	    }

	    D_line_width(width);
	}		/* end if nrec_width */


	/* enough of the prep work, lets start plotting stuff */
	x = Points->x;
	y = Points->y;

	if ((ltype & GV_POINTS) && (Symb != NULL || (nrec_size || nrec_rot)) ) {
	    if (!(color || fcolor || custom_rgb))
		continue;

	    G_plot_where_xy(x[0], y[0], &x0, &y0);

	    /* skip if the point is outside of the display window */
	    /*      xy<0 tests make it go ever-so-slightly faster */
	    if (x0 < 0 || y0 < 0 ||
		x0 > D_get_d_east() || x0 < D_get_d_west() ||
		y0 > D_get_d_south() || y0 < D_get_d_north())
		continue;

	    /* dynamic symbol size */
	    if (nrec_size) {
		/* only first category */
		Vect_cat_get(Cats,
			     (Clist->field > 0 ? Clist->field :
			      (Cats->n_cats > 0 ?
			       Cats->field[0] : 1)), &cat);

		if (cat >= 0) {
		    G_debug(3, "display element %d, cat %d", line, cat);

		    /* Read symbol size from db for current symbol # */
		    if (db_CatValArray_get_value(&cvarr_size, cat, &cv_size) !=
			DB_OK) {
			var_size = size;
		    }
		    else {
			var_size = size *
			    (cvarr_size.ctype == DB_C_TYPE_INT ?
			     (double)cv_size->val.i : cv_size->val.d);

			if (var_size < 0.0) {
			    G_warning(_("Error in symbol size column (%s), element %d "
					"with cat %d: symbol size [%f]"),
				      size_column, line, cat, var_size);
			    var_size = size;
			}
		    }
		}		/* end if cat */
		else {
		    var_size = size;
		}
	    }		/* end if nrec_size */

	    /* dynamic symbol rotation */
	    if (nrec_rot) {
		/* only first category */
		Vect_cat_get(Cats,
			     (Clist->field > 0 ? Clist->field :
			      (Cats->n_cats > 0 ?
			       Cats->field[0] : 1)), &cat);

		if (cat >= 0) {
		    G_debug(3, "display element %d, cat %d", line, cat);

		    /* Read symbol rotation from db for current symbol # */
		    if (db_CatValArray_get_value(&cvarr_rot, cat, &cv_rot) !=
			DB_OK) {
			rotation = 0.0;
		    }
		    else {
			rotation =
			    (cvarr_rot.ctype == DB_C_TYPE_INT ?
			     (double)cv_rot->val.i : cv_rot->val.d);
		    }
		}		/* end if cat */
		else {
		    rotation = 0.0;
		}
	    }		/* end if nrec_rot */

	    if(nrec_size || nrec_rot) {
		G_debug(3, ". dynamic symbol: cat=%d  size=%d  rotation=%.2f",
			cat, (int)(var_size + 0.5), rotation);

		/* symbol stroking is cumulative, so we need to reread it each time */
		if(Symb) /* unclean free() on first iteration if variables are not init'd to NULL? */
		    G_free(Symb);
		Symb = S_read(symbol_name);
		if (Symb == NULL)
		    G_warning(_("Unable to read symbol, unable to display points"));
		else
		    S_stroke(Symb, (int)(var_size + 0.5), rotation, 0);
	    }

	    /* use random or RGB column color if given, otherwise reset */
	    /* centroids always use default color to stand out from underlying area */
	    if (custom_rgb && (ltype != GV_CENTROID)) {
		primary_color->r = (unsigned char)red;
		primary_color->g = (unsigned char)grn;
		primary_color->b = (unsigned char)blu;
		D_symbol2(Symb, x0, y0, primary_color, line_color);
	    }
	    else
		D_symbol(Symb, x0, y0, line_color, fill_color);

	    /* reset to defaults */
	    var_size = size;
	    rotation = 0.0;
	}
	else if (color || custom_rgb || (z_color_flag && Vect_is_3d(Map))) {
	    if (!table_colors_flag && !cats_color_flag && !z_color_flag)
		R_RGB_color(color->r, color->g, color->b);
	    else {
		if (custom_rgb)
		    R_RGB_color((unsigned char)red, (unsigned char)grn,
				(unsigned char)blu);
		else
		    R_RGB_color(color->r, color->g, color->b);
	    }

	    /* Plot the lines */
	    if (Points->n_points == 1)	/* line with one coor */
		D_polydots_clip(x, y, Points->n_points);
	    else		/*use different user defined render methods */
		plot_polyline(x, y, Points->n_points);
	}
    }

    if (nerror_rgb > 0) {
	G_warning(_("Error in color definition column '%s': %d features affected"),
		  rgb_column, nerror_rgb);
    }
    
    Vect_destroy_line_struct(Points);
    Vect_destroy_cats_struct(Cats);

    return 0;			/* not reached */
}
