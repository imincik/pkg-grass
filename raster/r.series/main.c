
/****************************************************************************
 *
 * MODULE:       r.series
 * AUTHOR(S):    Glynn Clements <glynn gclements.plus.com> (original contributor)
 *               Hamish Bowman <hamish_b yahoo.com>, Jachym Cepicky <jachym les-ejk.cz>,
 *               Martin Wegmann <wegmann biozentrum.uni-wuerzburg.de>
 * PURPOSE:      
 * COPYRIGHT:    (C) 2002-2008 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <grass/gis.h>
#include <grass/glocale.h>
#include <grass/stats.h>

struct menu
{
    stat_func *method;		/* routine to compute new value */
    int is_int;			/* result is an integer */
    char *name;			/* method name */
    char *text;			/* menu display - full description */
} menu[] = {
    {c_ave,    0, "average",    "average value"},
    {c_count,  1, "count",      "count of non-NULL cells"},
    {c_median, 0, "median",     "median value"},
    {c_mode,   0, "mode",       "most frequently occuring value"},
    {c_min,    0, "minimum",    "lowest value"},
    {c_minx,   1, "min_raster", "raster with lowest value"},
    {c_max,    0, "maximum",    "highest value"},
    {c_maxx,   1, "max_raster", "raster with highest value"},
    {c_stddev, 0, "stddev",     "standard deviation"},
    {c_range,  0, "range",      "range of values"},
    {c_sum,    0, "sum",        "sum of values"},
    {c_thresh, 1, "threshold",  "threshold value"},
    {c_var,    0, "variance",   "statistical variance"},
    {c_divr,   1, "diversity",  "number of different values"},
    {c_reg_m,  0, "slope",      "linear regression slope"},
    {c_reg_c,  0, "offset",     "linear regression offset"},
    {c_reg_r2, 0, "detcoeff",   "linear regression coefficient of determination"},
    {c_quart1, 0, "quart1",     "first quartile"},
    {c_quart3, 0, "quart3",     "third quartile"},
    {c_perc90, 0, "perc90",     "ninetieth percentile"},
    {c_quant,  0, "quantile",   "arbitrary quantile"},
    {c_skew,   0, "skewness",   "skewness"},
    {c_kurt,   0, "kurtosis",   "kurtosis"},
    {NULL,     0, NULL,         NULL}
};

struct input
{
    const char *name, *mapset;
    int fd;
    DCELL *buf;
};

struct output
{
    const char *name;
    int fd;
    DCELL *buf;
    stat_func *method_fn;
    double quantile;
    double threshold;
};

static char *build_method_list(void)
{
    char *buf = G_malloc(1024);
    char *p = buf;
    int i;

    for (i = 0; menu[i].name; i++) {
	char *q;

	if (i)
	    *p++ = ',';
	for (q = menu[i].name; *q; p++, q++)
	    *p = *q;
    }
    *p = '\0';

    return buf;
}

static int find_method(const char *method_name)
{
    int i;

    for (i = 0; menu[i].name; i++)
	if (strcmp(menu[i].name, method_name) == 0)
	    return i;

    G_fatal_error(_("Unknown method <%s>"), method_name);

    return -1;
}

int main(int argc, char *argv[])
{
    struct GModule *module;
    struct
    {
	struct Option *input, *output, *method, *quantile, *threshold, *range;
    } parm;
    struct
    {
	/* please, remove before GRASS 7 released */
	struct Flag *quiet;
	struct Flag *nulls;
    } flag;
    int i;
    int num_inputs;
    struct input *inputs;
    int num_outputs;
    struct output *outputs;
    struct History history;
    DCELL *values, *values_tmp;
    int nrows, ncols;
    int row, col;
    double lo, hi;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, series");
    module->description =
	_("Makes each output cell value a "
	  "function of the values assigned to the corresponding cells "
	  "in the input raster map layers.");

    parm.input = G_define_standard_option(G_OPT_R_INPUTS);

    parm.output = G_define_standard_option(G_OPT_R_OUTPUT);
    parm.output->multiple = YES;

    parm.method = G_define_option();
    parm.method->key = "method";
    parm.method->type = TYPE_STRING;
    parm.method->required = YES;
    parm.method->options = build_method_list();
    parm.method->description = _("Aggregate operation");
    parm.method->multiple = YES;

    parm.quantile = G_define_option();
    parm.quantile->key = "quantile";
    parm.quantile->type = TYPE_DOUBLE;
    parm.quantile->required = NO;
    parm.quantile->description = _("Quantile to calculate for method=quantile");
    parm.quantile->options = "0.0-1.0";
    parm.quantile->multiple = YES;

    parm.threshold = G_define_option();
    parm.threshold->key = "threshold";
    parm.threshold->type = TYPE_DOUBLE;
    parm.threshold->required = NO;
    parm.threshold->description = _("Threshold to calculate for method=threshold");
    parm.threshold->multiple = YES;

    /* please, remove before GRASS 7 released */
    flag.quiet = G_define_flag();
    flag.quiet->key = 'q';
    flag.quiet->description = _("Run quietly");

    parm.range = G_define_option();
    parm.range->key = "range";
    parm.range->type = TYPE_DOUBLE;
    parm.range->key_desc = "lo,hi";
    parm.range->description = _("Ignore values outside this range");

    flag.nulls = G_define_flag();
    flag.nulls->key = 'n';
    flag.nulls->description = _("Propagate NULLs");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    /* please, remove before GRASS 7 released */
    if (flag.quiet->answer) {
	putenv("GRASS_VERBOSE=0");
	G_warning(_("The '-q' flag is superseded and will be removed "
		    "in future. Please use '--quiet' instead."));
    }

    if (parm.range->answer) {
	lo = atof(parm.range->answers[0]);
	hi = atof(parm.range->answers[1]);
    }

    /* process the input maps */
    for (i = 0; parm.input->answers[i]; i++) ;
    num_inputs = i;

    if (num_inputs < 1)
	G_fatal_error(_("No input raster map(s) specified"));

    inputs = G_malloc(num_inputs * sizeof(struct input));

    for (i = 0; i < num_inputs; i++) {
	struct input *p = &inputs[i];

	p->name = parm.input->answers[i];
	p->mapset = G_find_cell2(p->name, "");
	if (!p->mapset)
	    G_fatal_error(_("Raster map <%s> not found"), p->name);
	else
	    G_message(_("Reading raster map <%s>..."), p->name);
	p->fd = G_open_cell_old(p->name, p->mapset);
	if (p->fd < 0)
	    G_fatal_error(_("Unable to open raster map <%s> in mapset <%s>"),
			  p->name, p->mapset);
	p->buf = G_allocate_d_raster_buf();
    }

    /* process the output maps */
    for (i = 0; parm.output->answers[i]; i++) ;
    num_outputs = i;

    for (i = 0; parm.method->answers[i]; i++) ;
    if (num_outputs != i)
	G_fatal_error(_("output= and method= must have the same number of values"));

    outputs = G_calloc(num_outputs, sizeof(struct output));

    for (i = 0; i < num_outputs; i++) {
	struct output *out = &outputs[i];
	const char *output_name = parm.output->answers[i];
	const char *method_name = parm.method->answers[i];
	int method = find_method(method_name);

	out->name = output_name;
	out->method_fn = menu[method].method;
	out->quantile = (parm.quantile->answer && parm.quantile->answers[i])
	    ? atof(parm.quantile->answers[i])
	    : 0;
	out->threshold = (parm.threshold->answer && parm.threshold->answers[i])
	    ? atof(parm.threshold->answers[i])
	    : 0;
	out->buf = G_allocate_d_raster_buf();
	out->fd = G_open_raster_new(
	    output_name, menu[method].is_int ? CELL_TYPE : DCELL_TYPE);
	if (out->fd < 0)
	    G_fatal_error(_("Unable to create raster map <%s>"), out->name);
    }

    /* initialise variables */
    values = G_malloc(num_inputs * sizeof(DCELL));
    values_tmp = G_malloc(num_inputs * sizeof(DCELL));

    nrows = G_window_rows();
    ncols = G_window_cols();

    /* process the data */
    G_verbose_message(_("Percent complete..."));

    for (row = 0; row < nrows; row++) {
	G_percent(row, nrows, 2);

	for (i = 0; i < num_inputs; i++)
	    G_get_d_raster_row(inputs[i].fd, inputs[i].buf, row);

	for (col = 0; col < ncols; col++) {
	    int null = 0;

	    for (i = 0; i < num_inputs; i++) {
		DCELL v = inputs[i].buf[col];

		if (G_is_d_null_value(&v))
		    null = 1;
		else if (parm.range->answer && (v < lo || v > hi)) {
		    G_set_d_null_value(&v, 1);
		    null = 1;
		}

		values[i] = v;
	    }

	    for (i = 0; i < num_outputs; i++) {
		struct output *out = &outputs[i];

		if (null && flag.nulls->answer)
		    G_set_d_null_value(&out->buf[col], 1);
		else {
		    memcpy(values_tmp, values, num_inputs * sizeof(DCELL));
		    (*out->method_fn)(&out->buf[col], values_tmp, num_inputs, &out->threshold);
		}
	    }
	}

	for (i = 0; i < num_outputs; i++)
	    G_put_d_raster_row(outputs[i].fd, outputs[i].buf);
    }

    G_percent(row, nrows, 2);

    /* close maps */
    for (i = 0; i < num_outputs; i++) {
	struct output *out = &outputs[i];

	G_close_cell(out->fd);

	G_short_history(out->name, "raster", &history);
	G_command_history(&history);
	G_write_history(out->name, &history);
    }

    for (i = 0; i < num_inputs; i++)
	G_close_cell(inputs[i].fd);

    exit(EXIT_SUCCESS);
}
