
/****************************************************************************
 *
 * MODULE:       r.quantile
 * AUTHOR(S):    Glynn Clements <glynn gclements.plus.com> (original contributor),
 * PURPOSE:      Compute quantiles using two passes
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <grass/gis.h>
#include <grass/glocale.h>

struct bin
{
    unsigned long origin;
    DCELL min, max;
    int base, count;
};

static int rows, cols;

static DCELL min, max;
static int num_quants;
static DCELL *quants;
static int num_slots;
static unsigned int *slots;
static DCELL slot_size;
static unsigned long total;
static int num_values;
static unsigned char *slot_bins;
static int num_bins;
static struct bin *bins;
static DCELL *values;

static inline int get_slot(DCELL c)
{
    int i = (int)floor((c - min) / slot_size);

    if (i < 0)
	i = 0;
    if (i > num_slots - 1)
	i = num_slots - 1;
    return i;
}

static inline double get_quantile(int n)
{
    return (double)total * quants[n];
}

static void get_slot_counts(int infile)
{
    DCELL *inbuf = G_allocate_d_raster_buf();
    int row, col;

    G_message(_("Computing histogram"));

    total = 0;

    for (row = 0; row < rows; row++) {
	G_get_d_raster_row(infile, inbuf, row);

	for (col = 0; col < cols; col++) {
	    int i;

	    if (G_is_d_null_value(&inbuf[col]))
		continue;

	    i = get_slot(inbuf[col]);

	    slots[i]++;
	    total++;
	}

	G_percent(row, rows, 2);
    }

    G_percent(rows, rows, 2);
    G_free(inbuf);
}

static void initialize_bins(void)
{
    int slot;
    double next;
    int bin = 0;
    unsigned long accum = 0;
    int quant = 0;

    G_message(_("Computing bins"));

    num_values = 0;
    next = get_quantile(quant);

    for (slot = 0; slot < num_slots; slot++) {
	unsigned int count = slots[slot];
	unsigned long accum2 = accum + count;

	if (accum2 > next) {
	    struct bin *b = &bins[bin];

	    slot_bins[slot] = ++bin;

	    b->origin = accum;
	    b->base = num_values;
	    b->count = 0;
	    b->min = min + slot_size * slot;
	    b->max = min + slot_size * (slot + 1);

	    while (accum2 > next)
		next = get_quantile(++quant);

	    num_values += count;
	}

	accum = accum2;
    }

    num_bins = bin;
}

static void fill_bins(int infile)
{
    DCELL *inbuf = G_allocate_d_raster_buf();
    int row, col;

    G_message(_("Binning data"));

    for (row = 0; row < rows; row++) {
	G_get_d_raster_row(infile, inbuf, row);

	for (col = 0; col < cols; col++) {
	    int i, bin;
	    struct bin *b;

	    if (G_is_d_null_value(&inbuf[col]))
		continue;

	    i = get_slot(inbuf[col]);
	    if (!slot_bins[i])
		continue;

	    bin = slot_bins[i] - 1;
	    b = &bins[bin];

	    values[b->base + b->count++] = inbuf[col];
	}

	G_percent(row, rows, 2);
    }

    G_percent(rows, rows, 2);
    G_free(inbuf);
}

static int compare_dcell(const void *aa, const void *bb)
{
    DCELL a = *(const DCELL *)aa;
    DCELL b = *(const DCELL *)bb;

    if (a < b)
	return -1;
    if (a > b)
	return 1;
    return 0;
}

static void sort_bins(void)
{
    int bin;

    G_message(_("Sorting bins"));

    for (bin = 0; bin < num_bins; bin++) {
	struct bin *b = &bins[bin];

	qsort(&values[b->base], b->count, sizeof(DCELL), compare_dcell);
	G_percent(bin, num_bins, 2);
    }
    G_percent(bin, num_bins, 2);

    G_message(_("Computing quantiles"));
}

static void compute_quantiles(int recode)
{
    int bin = 0;
    double prev_v = min;
    int quant;

    for (quant = 0; quant < num_quants; quant++) {
	struct bin *b;
	double next = get_quantile(quant);
	double k, v;
	int i0, i1;

	for (; bin < num_bins; bin++) {
	    b = &bins[bin];
	    if (b->origin + b->count >= next)
		break;
	}

	if (bin < num_bins) {
	    k = next - b->origin;
	    i0 = (int)floor(k);
	    i1 = (int)ceil(k);

	    if (i1 > b->count - 1)
		i1 = b->count - 1;

	    v = (i0 == i1)
		? values[b->base + i0]
		: values[b->base + i0] * (i1 - k) +
		  values[b->base + i1] * (k - i0);
	}
	else
	    v = max;

	if (recode)
	    printf("%f:%f:%i\n", prev_v, v, quant + 1);
	else
	    printf("%d:%f:%f\n", quant, 100 * quants[quant], v);

	prev_v = v;
    }

    if (recode)
	printf("%f:%f:%i\n", prev_v, max, num_quants + 1);
}

int main(int argc, char *argv[])
{
    struct GModule *module;
    struct
    {
	struct Option *input, *quant, *perc, *slots;
    } opt;
    struct {
	struct Flag *r;
    } flag;
    int recode;
    int infile;
    struct FPRange range;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster, statistics");
    module->description = _("Compute quantiles using two passes.");

    opt.input = G_define_standard_option(G_OPT_R_INPUT);

    opt.quant = G_define_option();
    opt.quant->key = "quantiles";
    opt.quant->type = TYPE_INTEGER;
    opt.quant->required = NO;
    opt.quant->description = _("Number of quantiles");
    opt.quant->answer = "4";

    opt.perc = G_define_option();
    opt.perc->key = "percentiles";
    opt.perc->type = TYPE_DOUBLE;
    opt.perc->required = NO;
    opt.perc->multiple = YES;
    opt.perc->description = _("List of percentiles");

    opt.slots = G_define_option();
    opt.slots->key = "bins";
    opt.slots->type = TYPE_INTEGER;
    opt.slots->required = NO;
    opt.slots->description = _("Number of bins to use");
    opt.slots->answer = "1000000";

    flag.r = G_define_flag();
    flag.r->key = 'r';
    flag.r->description = _("Generate recode rules based on quantile-defined intervals.");
 
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    num_slots = atoi(opt.slots->answer);
    recode = flag.r->answer;

    if (opt.perc->answer) {
	int i;

	for (i = 0; opt.perc->answers[i]; i++) ;
	num_quants = i;
	quants = G_calloc(num_quants, sizeof(DCELL));
	for (i = 0; i < num_quants; i++)
	    quants[i] = atof(opt.perc->answers[i]) / 100;
	qsort(quants, num_quants, sizeof(DCELL), compare_dcell);
    }
    else {
	int i;

	num_quants = atoi(opt.quant->answer) - 1;
	quants = G_calloc(num_quants, sizeof(DCELL));
	for (i = 0; i < num_quants; i++)
	    quants[i] = 1.0 * (i + 1) / (num_quants + 1);
    }

    infile = G_open_cell_old(opt.input->answer, "");
    if (infile < 0)
	G_fatal_error(_("Unable to open raster map <%s>"), opt.input->answer);

    G_read_fp_range(opt.input->answer, "", &range);
    G_get_fp_range_min_max(&range, &min, &max);

    slots = G_calloc(num_slots, sizeof(unsigned int));
    slot_bins = G_calloc(num_slots, sizeof(unsigned char));

    slot_size = (max - min) / num_slots;

    rows = G_window_rows();
    cols = G_window_cols();

    get_slot_counts(infile);

    bins = G_calloc(num_quants, sizeof(struct bin));
    initialize_bins();
    G_free(slots);

    values = G_calloc(num_values, sizeof(DCELL));
    fill_bins(infile);

    G_close_cell(infile);
    G_free(slot_bins);

    sort_bins();
    compute_quantiles(recode);

    return (EXIT_SUCCESS);
}
