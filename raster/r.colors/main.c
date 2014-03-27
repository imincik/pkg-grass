/****************************************************************************
 *
 * MODULE:       r.colors
 *
 * AUTHOR(S):    Michael Shapiro - CERL
 *               David Johnson
 *
 * PURPOSE:      Allows creation and/or modification of the color table
 *               for a raster map layer.
 *
 * COPYRIGHT:    (C) 2006-2008 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 ***************************************************************************/

/* main.c
 *
 * specify and print options added by DBA Systems, Inc.
 * update 10/99 for GRASS 5
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include "local_proto.h"

static char **rules;
static int nrules;

static int cmp(const void *aa, const void *bb)
{
    char *const *a = (char *const *)aa;
    char *const *b = (char *const *)bb;
    
    return strcmp(*a, *b);
}

static void scan_rules(void)
{
    char path[GPATH_MAX];

    sprintf(path, "%s/etc/colors", G_gisbase());

    rules = G__ls(path, &nrules);

    rules = G_realloc(rules, (nrules + 4) * sizeof(const char *));

    rules[nrules++] = G_store("random");
    rules[nrules++] = G_store("grey.eq");
    rules[nrules++] = G_store("grey.log");
    rules[nrules++] = G_store("rules");

    qsort(rules, nrules, sizeof(char *), cmp);
}

static char *rules_list(void)
{
    char *list = NULL;
    int size = 0;
    int len = 0;
    int i;

    for (i = 0; i < nrules; i++) {
	const char *name = rules[i];
	int n = strlen(name);

	if (size < len + n + 2) {
	    size = len + n + 200;
	    list = G_realloc(list, size);
	}

	if (len > 0)
	    list[len++] = ',';

	memcpy(&list[len], name, n + 1);
	len += n;
    }

    return list;
}

static char *rules_descriptions(void)
{
    char path[GPATH_MAX];
    struct Key_Value *kv;
    int result_len = 0;
    int result_max = 2000;
    char *result = G_malloc(result_max);
    int stat;
    int i;

    sprintf(path, "%s/etc/colors.desc", G_gisbase());
    kv = G_read_key_value_file(path, &stat);
    if (!kv || stat < 0)
	return NULL;

    for (i = 0; i < nrules; i++) {
	const char *name = rules[i];
	const char *desc = G_find_key_value(name, kv);
	int len;

	if (!desc)
	    desc = "no description";

	desc = _(desc);

	len = strlen(name) + strlen(desc) + 2;
	if (result_len + len >= result_max) {
	    result_max = result_len + len + 1000;
	    result = G_realloc(result, result_max);
	}

	sprintf(result + result_len, "%s;%s;", name, desc);
	result_len += len;
    }

    G_free_key_value(kv);

    return result;
}

static void list_rules(void)
{
    int i;

    for (i = 0; i < nrules; i++)
	printf("%s\n", rules[i]);
}

static int find_rule(const char *name)
{
    int i;

    for (i = 0; i < nrules; i++)
	if (strcmp(name, rules[i]) == 0)
	    return 1;

    return 0;
}

int main(int argc, char **argv)
{
    int overwrite;
    int interactive;
    int remove;
    int have_colors;
    struct Colors colors, colors_tmp;
    struct Cell_stats statf;
    int have_stats = 0;
    struct FPRange range;
    DCELL min, max;
    char *name, *mapset;
    char *style, *cmap, *cmapset;
    char *rules;
    int fp;
    struct GModule *module;
    struct
    {
	struct Flag *r, *w, *l, *g, *a, *e, *i, *q, *n;
    } flag;
    struct
    {
	struct Option *map, *colr, *rast, *rules;
    } opt;


    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, color table");
    module->description =
	_("Creates/modifies the color table associated with a raster map layer.");

    opt.map = G_define_standard_option(G_OPT_R_MAP);
    opt.map->required = NO;
    opt.map->guisection = _("Required");

    scan_rules();

    opt.colr = G_define_option();
    opt.colr->key = "color";
    opt.colr->key_desc = "style";
    opt.colr->type = TYPE_STRING;
    opt.colr->required = NO;
    opt.colr->options = rules_list();
    opt.colr->description = _("Type of color table");
    opt.colr->descriptions = rules_descriptions();
    opt.colr->guisection = _("Colors");

    opt.rast = G_define_option();
    opt.rast->key = "raster";
    opt.rast->type = TYPE_STRING;
    opt.rast->required = NO;
    opt.rast->gisprompt = "old,cell,raster";
    opt.rast->description =
	_("Raster map name from which to copy color table");

    opt.rules = G_define_standard_option(G_OPT_F_INPUT);
    opt.rules->key = "rules";
    opt.rules->required = NO;
    opt.rules->description = _("Path to rules file (\"-\" to read rules from stdin)");
    opt.rules->guisection = _("Colors");

    flag.r = G_define_flag();
    flag.r->key = 'r';
    flag.r->description = _("Remove existing color table");

    flag.w = G_define_flag();
    flag.w->key = 'w';
    flag.w->description =
	_("Only write new color table if one doesn't already exist");

    flag.l = G_define_flag();
    flag.l->key = 'l';
    flag.l->description = _("List available rules then exit");

    flag.n = G_define_flag();
    flag.n->key = 'n';
    flag.n->description = _("Invert colors");
    flag.n->guisection = _("Colors");

    flag.g = G_define_flag();
    flag.g->key = 'g';
    flag.g->description = _("Logarithmic scaling");
    flag.g->guisection = _("Colors");

    flag.a = G_define_flag();
    flag.a->key = 'a';
    flag.a->description = _("Logarithmic-absolute scaling");
    flag.a->guisection = _("Colors");

    flag.e = G_define_flag();
    flag.e->key = 'e';
    flag.e->description = _("Histogram equalization");
    flag.e->guisection = _("Colors");

    flag.i = G_define_flag();
    flag.i->key = 'i';
    flag.i->description = _("Enter rules interactively");

    /* please, remove before GRASS 7 released */
    flag.q = G_define_flag();
    flag.q->key = 'q';
    flag.q->description = _("Run quietly");


    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* please, remove before GRASS 7 released */
    if (flag.q->answer) {
	G_putenv("GRASS_VERBOSE", "0");
	G_warning(_("The '-q' flag is superseded and will be removed "
		    "in future. Please use '--quiet' instead."));
    }

    if (flag.l->answer) {
	list_rules();
	return EXIT_SUCCESS;
    }

    overwrite = !flag.w->answer;
    interactive = flag.i->answer;
    remove = flag.r->answer;

    name = opt.map->answer;

    style = opt.colr->answer;
    cmap = opt.rast->answer;
    rules = opt.rules->answer;

    if (!name)
	G_fatal_error(_("No raster map specified"));

    if (!cmap && !style && !rules && !interactive && !remove)
	G_fatal_error(_("One of \"-i\" or \"-r\" or options \"color\", \"rast\" or \"rules\" must be specified!"));

    if (interactive && (style || rules || cmap))
	G_fatal_error(_("Interactive mode is incompatible with \"color\", \"rules\", and \"raster\" options"));

    if ((style && (cmap || rules)) || (cmap && rules)) {
	if ((style && rules && !cmap) && strcmp(style, "rules") == 0)
	    style = NULL;
	else
	    G_fatal_error(
		_("\"color\", \"rules\", and \"raster\" options are mutually exclusive"));
    }

    /* handle rules="-" (from stdin) by translating that to colors=rules */
    /* this method should not be ported to GRASS 7 verbatim, as color=rules DNE */
    if (rules && strcmp(rules, "-") == 0) {
	style = G_store("rules");
	rules = NULL;
    }

    if (flag.g->answer && flag.a->answer)
	G_fatal_error(_("-g and -a flags are mutually exclusive"));

    mapset = G_find_cell2(name, "");
    if (mapset == NULL)
	G_fatal_error(_("Raster map <%s> not found"), name);

    if (remove) {
	int stat = G_remove_colors(name, mapset);

	if (stat < 0)
	    G_fatal_error(_("Unable to remove color table of raster map <%s>"), name);
	if (stat == 0)
	    G_warning(_("Color table of raster map <%s> not found"), name);
	return EXIT_SUCCESS;
    }

    G_suppress_warnings(1);
    have_colors = G_read_colors(name, mapset, &colors);
    /*if (have_colors >= 0)
       G_free_colors(&colors); */

    if (have_colors > 0 && !overwrite) {
	G_warning(_("Color table exists. Exiting."));
	exit(EXIT_FAILURE);
    }

    G_suppress_warnings(0);

    fp = G_raster_map_is_fp(name, mapset);
    G_read_fp_range(name, mapset, &range);
    G_get_fp_range_min_max(&range, &min, &max);

    if (interactive) {
	if (!read_color_rules(stdin, &colors, min, max, fp))
	    exit(EXIT_FAILURE);
    }
    else if (style) {
	/* 
	 * here the predefined color-table color-styles are created by GRASS library calls. 
	 */
	if (strcmp(style, "random") == 0) {
	    if (fp)
		G_fatal_error(_("Color table 'random' is not supported for floating point raster map"));
	    G_make_random_colors(&colors, (CELL) min, (CELL) max);
	}
	else if (strcmp(style, "grey.eq") == 0) {
	    if (fp)
		G_fatal_error(_("Color table 'grey.eq' is not supported for floating point raster map"));
	    if (!have_stats)
		have_stats = get_stats(name, mapset, &statf);
	    G_make_histogram_eq_colors(&colors, &statf);
	}
	else if (strcmp(style, "grey.log") == 0) {
	    if (fp)
		G_fatal_error(_("Color table 'grey.log' is not supported for floating point raster map"));
	    if (!have_stats)
		have_stats = get_stats(name, mapset, &statf);
	    G_make_histogram_log_colors(&colors, &statf, (CELL) min,
					(CELL) max);
	}
	else if (strcmp(style, "rules") == 0) {
	    if (!read_color_rules(stdin, &colors, min, max, fp))
		exit(EXIT_FAILURE);
	}
	else if (find_rule(style))
	    G_make_fp_colors(&colors, style, min, max);
	else
	    G_fatal_error(_("Unknown color request '%s'"), style);
    }
    else if (rules) {
	if (!G_load_fp_colors(&colors, rules, min, max)) {
	    /* for backwards compatibility try as std name; remove for GRASS 7 */
	    char path[GPATH_MAX];

	    /* don't bother with native dirsep as not needed for backwards compatibility */
	    sprintf(path, "%s/etc/colors/%s", G_gisbase(), rules);

	    if (!G_load_fp_colors(&colors, path, min, max))
		G_fatal_error(_("Unable to load rules file <%s>"), rules);
	}
    }
    else {
	/* use color from another map (cmap) */
	cmapset = G_find_cell2(cmap, "");
	if (cmapset == NULL)
	    G_fatal_error(_("Raster map <%s> not found"), cmap);

	if (G_read_colors(cmap, cmapset, &colors) < 0)
	    G_fatal_error(_("Unable to read color table for raster map <%s>"), cmap);
    }

    if (fp)
	G_mark_colors_as_fp(&colors);

    if (flag.n->answer)
	G_invert_colors(&colors);

    if (flag.e->answer) {
	if (fp) {
	    struct FP_stats fpstats;
	    get_fp_stats(name, mapset, &fpstats, min, max, flag.g->answer, flag.a->answer);
	    G_histogram_eq_colors_fp(&colors_tmp, &colors, &fpstats);
	}
	else {
	    if (!have_stats) 
		have_stats = get_stats(name, mapset, &statf);
	    G_histogram_eq_colors(&colors_tmp, &colors, &statf);
	}
	colors = colors_tmp;
    }

    if (flag.g->answer) {
	G_log_colors(&colors_tmp, &colors, 100);
	colors = colors_tmp;
    }

    if (flag.a->answer) {
	G_abs_log_colors(&colors_tmp, &colors, 100);
	colors = colors_tmp;
    }

    if (fp)
	G_mark_colors_as_fp(&colors);

    if (G_write_colors(name, mapset, &colors) >= 0)
	G_message(_("Color table for raster map <%s> set to '%s'"), name,
		  interactive ? "rules" : style ? style : rules ? rules :
		  cmap);

    exit(EXIT_SUCCESS);
}
