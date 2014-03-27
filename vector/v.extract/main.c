
/****************************************************************
 *
 * MODULE:     v.extract
 * 
 * AUTHOR(S):  R.L.Glenn , Soil Conservation Service, USDA
 *             update to 5.7:  Radim Blazek
 *               
 * PURPOSE:    Provides a means of generating vector (digit) files
 *             from an existing vector maplayer. Selects all vector
 *             boundaries for 1 or several areas of a list of
 *             user provided categories.
 *
 * COPYRIGHT:  (C) 2002-2007 by the GRASS Development Team
 *
 *             This program is free software under the 
 *             GNU General Public License (>=v2). 
 *             Read the file COPYING that comes with GRASS
 *             for details.
 *
 * TODO:       - fix white space problems for file= option
 ****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <search.h>
#include <sys/types.h>
#include <unistd.h>
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/dbmi.h>
#include <grass/gmath.h>
#include <grass/glocale.h>

static int *cat_array, cat_count, cat_size;
int scan_cats(char *, int *, int *);
int xtract_line(int, int[], struct Map_info *, struct Map_info *, int, int,
		int, int, int, int);

static void add_cat(int x)
{
    G_debug(2, "add_cat %d", x);

    if (cat_count >= cat_size) {
	cat_size = (cat_size < 1000) ? 1000 : cat_size * 2;
	cat_array = G_realloc(cat_array, cat_size * sizeof(int));
    }

    cat_array[cat_count++] = x;
}

/* Comparison function for *search */
static int cmp(const void *pa, const void *pb)
{
    int *p1 = (int *)pa;
    int *p2 = (int *)pb;

    if (*p1 < *p2)
	return -1;
    if (*p1 > *p2)
	return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int i, new_cat, type, ncats, *cats, field, c;
    int **ocats, *nocats, nfields, *fields;
    int dissolve = 0, x, y, type_only;
    char buffr[1024], text[80];
    char *input, *output, *mapset;
    struct GModule *module;
    struct Option *inopt, *outopt, *fileopt, *newopt, *typopt, *listopt,
	*fieldopt, *whereopt, *nrandopt;
    struct Flag *t_flag, *d_flag, *r_flag;
    struct Map_info In, Out;
    struct field_info *Fi;
    FILE *in;
    dbDriver *driver;
    dbHandle handle;
    struct line_cats *Cats;
    struct Cat_index *ci;
    int ucat_count, *ucat_array = NULL, prnd, seed, nrandom, nfeatures;

    G_gisinit(argv[0]);

    /* set up the options and flags for the command line parser */
    module = G_define_module();
    module->keywords = _("vector, extract");
    module->description =
	_("Selects vector objects from an existing vector map and "
	  "creates a new map containing only the selected objects.");

    d_flag = G_define_flag();
    d_flag->key = 'd';
    d_flag->description = _("Dissolve common boundaries (default is no)");

    t_flag = G_define_flag();
    t_flag->key = 't';
    t_flag->description = _("Do not copy table (see also 'new' parameter)");

    r_flag = G_define_flag();
    r_flag->key = 'r';
    r_flag->description = _("Reverse selection");
    r_flag->guisection = _("Selection");

    inopt = G_define_standard_option(G_OPT_V_INPUT);

    outopt = G_define_standard_option(G_OPT_V_OUTPUT);

    typopt = G_define_standard_option(G_OPT_V_TYPE);
    typopt->answer = "point,line,boundary,centroid,area,face";
    typopt->options = "point,line,boundary,centroid,area,face";
    typopt->label = _("Types to be extracted");
    typopt->guisection = _("Selection");

    fieldopt = G_define_standard_option(G_OPT_V_FIELD);
    fieldopt->gisprompt = "old_layer,layer,layer_all";
    fieldopt->label =
	_("Layer number (if -1, all features in all layers of given type " "are extracted)");
    fieldopt->guisection = _("Selection");
    
    listopt = G_define_standard_option(G_OPT_V_CATS);
    listopt->key = "list";
    listopt->guisection = _("Selection");

    whereopt = G_define_standard_option(G_OPT_WHERE);
    whereopt->guisection = _("Selection");

    fileopt = G_define_standard_option(G_OPT_F_INPUT);
    fileopt->key = "file";
    fileopt->required = NO;
    fileopt->label =
	_("Input text file with category numbers/number ranges to be extracted");
    fileopt->description = _("If '-' given reads from standard input");
    fileopt->guisection = _("Selection");

    nrandopt = G_define_option();
    nrandopt->key = "random";
    nrandopt->type = TYPE_INTEGER;
    nrandopt->required = NO;
    nrandopt->label =
	_("Number of random categories matching vector objects to extract");
    nrandopt->description =
	_("Number must be smaller than unique cat count in layer");
    nrandopt->guisection = _("Selection");

    newopt = G_define_option();
    newopt->key = "new";
    newopt->type = TYPE_INTEGER;
    newopt->required = NO;
    newopt->answer = "-1";
    newopt->label =
	_("Enter -1 to keep original categories or the desired NEW category value");
    newopt->description = _("If new >= 0, table is not copied");

    /* heeeerrrrrre's the   PARSER */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);


    /* start checking options and flags */
    c = 0;
    if (fileopt->answer != NULL)
	c++;
    if (listopt->answers != NULL)
	c++;
    if (whereopt->answer != NULL)
	c++;
    if (nrandopt->answer != NULL)
	c++;
    if (c > 1)
	G_fatal_error(_("List, file, where and random options are exclusive. "
			"Please specify only one of them"));
    c = 0;

    type_only = 0;
    if (!listopt->answers && !fileopt->answer && !whereopt->answer &&
	!nrandopt->answer) {
	type_only = 1;
    }

    Vect_check_input_output_name(inopt->answer, outopt->answer,
				 GV_FATAL_EXIT);

    /* set input vector map name and mapset */
    input = inopt->answer;
    mapset = G_find_vector2(input, "");

    if (!mapset)
	G_fatal_error(_("Vector map <%s> not found"), input);

    G_debug(3, "Mapset = %s", mapset);
    /* set output vector map name */
    output = outopt->answer;

    if (d_flag->answer)
	dissolve = 1;

    field = atoi(fieldopt->answer);
    if (field == 0)
	G_fatal_error(_("Layer 0 not supported"));

    if (!newopt->answer)
	new_cat = 0;
    else
	new_cat = atoi(newopt->answer);

    /* Do initial read of input file */
    Vect_set_open_level(2);
    Vect_open_old(&In, input, mapset);

    type = Vect_option_to_types(typopt);
    if (type & GV_AREA) {
	type |= GV_CENTROID;
    }

    /* Read categoy list */
    cat_count = 0;
    if (listopt->answer != NULL) {
	/* no file of categories to read, process cat list */
	/* first check for valid list */
	for (i = 0; listopt->answers[i]; i++) {
	    G_debug(2, "catlist item: %s", listopt->answers[i]);
	    if (!scan_cats(listopt->answers[i], &x, &y))
		G_fatal_error(_("Category value in '%s' not valid"),
			      listopt->answers[i]);
	}

	/* valid list, put into cat value array */
	for (i = 0; listopt->answers[i]; i++) {
	    scan_cats(listopt->answers[i], &x, &y);
	    while (x <= y)
		add_cat(x++);
	}
    }
    else if (fileopt->answer != NULL) {	/* got a file of category numbers */
	if (G_strcasecmp(fileopt->answer, "-") == 0) {
	    in = stdin;
	}
	else {
	    G_message(_("Process file <%s> for category numbers"),
		      fileopt->answer);

	    /* open input file */
	    if ((in = fopen(fileopt->answer, "r")) == NULL)
		G_fatal_error(_("Unable to open specified file <%s>"),
			      fileopt->answer);
	}

	while (1) {
	    if (!fgets(buffr, 39, in))
		break;
	    G_chop(buffr);	/* eliminate some white space, we accept numbers and dashes only */
	    sscanf(buffr, "%[-0-9]", text);
	    if (strlen(text) == 0)
		G_warning(_("Ignored text entry: %s"), buffr);

	    scan_cats(text, &x, &y);
	    /* two BUGS here: - fgets stops if white space appears
	       - if white space appears, number is not read correctly */
	    while (x <= y && x >= 0 && y >= 0)
		add_cat(x++);
	}

	if (G_strcasecmp(fileopt->answer, "-") != 0)
	    fclose(in);

    }
    else if (whereopt->answer != NULL) {
	if (field < 1) {
	    G_fatal_error(_("'layer' must be > 0 for 'where'."));
	}
	Fi = Vect_get_field(&In, field);
	if (!Fi) {
	    G_fatal_error(_("Database connection not defined for layer %d"),
			  field);
	}

	G_debug(1, "Loading categories from table <%s>", Fi->table);

	driver = db_start_driver(Fi->driver);
	if (driver == NULL)
	    G_fatal_error(_("Unable to start driver <%s>"), Fi->driver);

	db_init_handle(&handle);
	db_set_handle(&handle, Fi->database, NULL);
	if (db_open_database(driver, &handle) != DB_OK)
	    G_fatal_error(_("Unable to open database <%s> by driver <%s>"),
			  Fi->database, Fi->driver);

	ncats =db_select_int(driver, Fi->table, Fi->key, whereopt->answer,
			  &cats);
	if (ncats == -1)
		G_fatal_error(_("Unable select records from table <%s>"), Fi->table);
	G_message(_("%d categories loaded from table <%s>"), ncats,
		  Fi->table);

	db_close_database(driver);
	db_shutdown_driver(driver);

	for (i = 0; i < ncats; i++)
	    add_cat(cats[i]);
	if (ncats >= 0)
	    G_free(cats);
    }
    else if (nrandopt->answer != NULL) {	/* Generate random category list */

	/* We operate on layer's CAT's and thus valid layer is required */
	if (Vect_cidx_get_field_index(&In, field) < 0)
	    G_fatal_error(_("This map has no categories attached. "
			    "Use v.category to attach categories to this vector map."));

	/* Don't do any processing, if user input is wrong */
	nrandom = atoi(nrandopt->answer);
	if (nrandom < 1)
	    G_fatal_error(_("Please specify random number larger than 0"));

	nfeatures = Vect_cidx_get_type_count(&In, field, type);
	if (nrandom >= nfeatures)
	    G_fatal_error(_("Random category count must be smaller than feature count. "
			   "There are only %d features of type(s): %s"),
			  nfeatures, typopt->answer);

	/* Let's create an array of uniq CAT values
	   According to Vlib/build.c, cidx should be allready sorted by dig_cidx_sort() */
	ci = &(In.plus.cidx[Vect_cidx_get_field_index(&In, field)]);
	ucat_count = 0;
	for (c = 0; c < ci->n_cats; c++) {
	    /* Bitwise AND compares ci feature type with user's requested types */
	    if (ci->cat[c][1] & type) {
		/* Don't do anything if such value allready exists */
		if (ucat_count > 0 &&
		    ucat_array[ucat_count - 1] == ci->cat[c][0])
		    continue;
		ucat_array =
		    G_realloc(ucat_array, (ucat_count + 1) * sizeof(int));
		ucat_array[ucat_count] = ci->cat[c][0];
		ucat_count++;
	    }
	}

	if (nrandom >= ucat_count)
	    G_fatal_error(_("Random category count is larger or equal to uniq \"%s\" feature category count %d"),
			  typopt->answer, ucat_count);

	if (ucat_count >= RAND_MAX)
	    G_fatal_error
		("There are more categories than random number generator can reach. "
		 "Report this as a GRASS bug.");

	seed = getpid();
	/* Initialise random number generator */
	G_math_rand(-1 * seed);

	/* Fill cat_array with list of valid random numbers */
	while (cat_count < nrandom) {
	    /* Random number in range from 0 to largest CAT value */
	    prnd =
		(int)floor(G_math_rand(seed) *
			   (ucat_array[ucat_count - 1] + 1));
	    qsort(cat_array, cat_count, sizeof(int), cmp);
	    /* Check if generated number isn't already in final list and is in list of existing CATs */
	    if (bsearch(&prnd, cat_array, cat_count, sizeof(int), cmp) == NULL
		&& bsearch(&prnd, ucat_array, ucat_count, sizeof(int),
			   cmp) != NULL)
		add_cat(prnd);
	}
	G_free(ucat_array);
	qsort(cat_array, cat_count, sizeof(int), cmp);
    }

    /* Open output file only when it's required */
    Vect_open_new(&Out, output, Vect_is_3d(&In));
    Vect_hist_copy(&In, &Out);
    Vect_hist_command(&Out);

    /* Read and write header info */
    Vect_copy_head_data(&In, &Out);
    
    G_message(_("Extracting features..."));
    xtract_line(cat_count, cat_array, &In, &Out, new_cat, type, dissolve,
		field, type_only, r_flag->answer ? 1 : 0);

    Vect_build(&Out);

    /* Copy tables */
    if (!t_flag->answer) {
	int nlines, line;
	int ttype, ntabs = 0;
	struct field_info *IFi, *OFi;

	/* Collect list of output cats */
	Cats = Vect_new_cats_struct();
	nfields = Vect_cidx_get_num_fields(&Out);
	ocats = (int **)G_malloc(nfields * sizeof(int *));
	nocats = (int *)G_malloc(nfields * sizeof(int));
	fields = (int *)G_malloc(nfields * sizeof(int));
	for (i = 0; i < nfields; i++) {
	    nocats[i] = 0;
	    ocats[i] =
		(int *)G_malloc(Vect_cidx_get_num_cats_by_index(&Out, i) *
				sizeof(int));
	    fields[i] = Vect_cidx_get_field_number(&Out, i);
	}

	nlines = Vect_get_num_lines(&Out);
	for (line = 1; line <= nlines; line++) {
	    Vect_read_line(&Out, NULL, Cats, line);

	    for (i = 0; i < Cats->n_cats; i++) {
		int f, j;

		for (j = 0; j < nfields; j++) {	/* find field */
		    if (fields[j] == Cats->field[i]) {
			f = j;
			break;
		    }
		}
		ocats[f][nocats[f]] = Cats->cat[i];
		nocats[f]++;
	    }
	}

	/* Copy tables */
	G_message(_("Writing attributes..."));

	/* Number of output tabs */
	for (i = 0; i < Vect_get_num_dblinks(&In); i++) {
	    int j, f = -1;

	    IFi = Vect_get_dblink(&In, i);

	    for (j = 0; j < nfields; j++) {	/* find field */
		if (fields[j] == IFi->number) {
		    f = j;
		    break;
		}
	    }
	    if (f >= 0 && nocats[f] > 0)
		ntabs++;
	}

	if (ntabs > 1)
	    ttype = GV_MTABLE;
	else
	    ttype = GV_1TABLE;

	for (i = 0; i < nfields; i++) {
	    int ret;

	    if (fields[i] == 0)
		continue;
	    if (nocats[i] == 0)
		continue;
	    if (fields[i] == field && new_cat != -1)
		continue;

	    G_verbose_message(_("Layer %d"), fields[i]);

	    /* Make a list of categories */
	    IFi = Vect_get_field(&In, fields[i]);
	    if (!IFi) {		/* no table */
		G_verbose_message(_("No table"));
		continue;
	    }

	    OFi =
		Vect_default_field_info(&Out, IFi->number, IFi->name, ttype);

	    ret =
		db_copy_table_by_ints(IFi->driver, IFi->database, IFi->table,
				      OFi->driver,
				      Vect_subst_var(OFi->database, &Out),
				      OFi->table, IFi->key, ocats[i],
				      nocats[i]);

	    if (ret == DB_FAILED) {
		G_warning(_("Unable to copy table"));
	    }
	    else {
		Vect_map_add_dblink(&Out, OFi->number, OFi->name, OFi->table,
				    IFi->key, OFi->database, OFi->driver);
	    }
	    G_done_msg(" ");
	}
    }

    Vect_close(&In);


    /* remove duplicate centroids */
    if (dissolve) {
	int line, nlines, ltype, area;

	G_message(_("Removing duplicate centroids..."));
	nlines = Vect_get_num_lines(&Out);
	for (line = 1; line <= nlines; line++) {
	    if (!Vect_line_alive(&Out, line))
		continue;	/* should not happen */

	    ltype = Vect_read_line(&Out, NULL, NULL, line);
	    if (!(ltype & GV_CENTROID)) {
		continue;
	    }
	    area = Vect_get_centroid_area(&Out, line);

	    if (area < 0) {
		Vect_delete_line(&Out, line);
	    }
	}
	Vect_build_partial(&Out, GV_BUILD_NONE);
	Vect_build(&Out);
    }

    Vect_close(&Out);

    exit(EXIT_SUCCESS);
}



int scan_cats(char *s, int *x, int *y)
{
    char dummy[2];

    if (strlen(s) == 0) {	/* error */
	*y = *x = -1;
	return 1;
    }

    *dummy = 0;
    if (sscanf(s, "%d-%d%1s", x, y, dummy) == 2)
	return (*dummy == 0 && *x <= *y);

    *dummy = 0;
    if (sscanf(s, "%d%1s", x, dummy) == 1 && *dummy == 0) {
	*y = *x;
	return 1;
    }
    return 0;
}
