
/****************************************************************************
 *
 * MODULE:	r.li.pielou
 *
 * PURPOSE:	brief calculates pielou's diversity index
 *
 * AUTHOR(S):	Luca Delucchi, Fondazione Edmund Mach         
 *		Serena Pallecchi student of Computer Science University of Pisa (Italy)
 *            	
 * COPYRIGHT:
 *		This program is free software under the GPL (>=v2)
 *		Read the COPYING file that comes with GRASS for details.
 *       
 ***************************************************************************/

#include <stdlib.h>
#include <fcntl.h>
#include <math.h>

#include <grass/gis.h>
#include <grass/glocale.h>

#include "../r.li.daemon/defs.h"
#include "../r.li.daemon/avlDefs.h"
#include "../r.li.daemon/avl.h"
#include "../r.li.daemon/daemon.h"

int calculate(int fd, area_des ad, double *result);

int calculateD(int fd, area_des ad, double *result);

int calculateF(int fd, area_des ad, double *result);

/*This function is used to sort the values in the moving window */
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

int main(int argc, char *argv[])
{
    struct Option *raster, *conf, *output;

    struct GModule *module;

    G_gisinit(argv[0]);
    module = G_define_module();
    module->description =
	_("Calculates Pielou's diversity index on a raster map");
    module->keywords =
	_("raster, landscape structure analysis, diversity index");

    /* define options */

    raster = G_define_standard_option(G_OPT_R_MAP);

    conf = G_define_option();
    conf->key = "conf";
    conf->description = _("Configuration file");
    conf->type = TYPE_STRING;
    conf->gisprompt = "old_file,file,input";
    conf->required = YES;

    output = G_define_standard_option(G_OPT_R_OUTPUT);

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    return calculateIndex(conf->answer, pielou, NULL, raster->answer,
			  output->answer);

}

int pielou(int fd, char **par, area_des ad, double *result)
{

    char *mapset;
    int ris = RLI_OK;
    double indice = 0;
    struct Cell_head hd;


    mapset = G_find_cell(ad->raster, "");
    if (G_get_cellhd(ad->raster, mapset, &hd) == -1)
	return RLI_ERRORE;


    switch (ad->data_type) {
    case CELL_TYPE:
	{
	    calculate(fd, ad, &indice);
	    break;
	}
    case DCELL_TYPE:
	{
	    calculateD(fd, ad, &indice);
	    break;
	}
    case FCELL_TYPE:
	{
	    calculateF(fd, ad, &indice);
	    break;
	}
    default:
	{
	    G_fatal_error("data type unknown");
	    return RLI_ERRORE;
	}
    }

    if (ris != RLI_OK) {
	return RLI_ERRORE;
    }

    *result = indice;

    return RLI_OK;

}


int calculate(int fd, area_des ad, double *result)
{

    CELL *buf;
    CELL corrCell;
    CELL precCell;
    int i, j;
    int mask_fd = -1, *mask_buf;
    int ris = 0;
    int masked = FALSE;
    int a = 0;			/* a=0 if all cells are null */
    long m = 0;
    long tot = 0;
    long zero = 0;
    long totCorr = 0;
    double indice = 0;
    double somma = 0;
    double percentuale = 0;
    double area = 0;
    double t;
    double logaritmo;

    /* add to compute the number of class */
    long *array_copy;
    int totNumClass = 1;
    double logTotNumClass;

    generic_cell cc;

    generic_cell cc_passage;	/*add to compute the number of class */

    avl_tree albero = NULL;

    AVL_table *array;

    cc.t = CELL_TYPE;

    /* open mask if needed */
    if (ad->mask == 1) {
	if ((mask_fd = open(ad->mask_name, O_RDONLY, 0755)) < 0)
	    return RLI_ERRORE;
	mask_buf = G_malloc(ad->cl * sizeof(int));
	if (mask_buf == NULL) {
	    G_fatal_error("malloc mask_buf failed");
	    return RLI_ERRORE;
	}
	masked = TRUE;
    }


    G_set_c_null_value(&precCell, 1);


    /*for each row */
    for (j = 0; j < ad->rl; j++) {
	buf = RLI_get_cell_raster_row(fd, j + ad->y, ad);


	if (masked) {
	    if (read(mask_fd, mask_buf, (ad->cl * sizeof(int))) < 0) {
		G_fatal_error("mask read failed");
		return RLI_ERRORE;
	    }
	}


	for (i = 0; i < ad->cl; i++) {	/* for each cell in the row */
	    area++;
	    corrCell = buf[i + ad->x];

	    if (masked && mask_buf[i + ad->x] == 0) {
		G_set_c_null_value(&corrCell, 1);
		area--;
	    }

	    if (!(G_is_null_value(&corrCell, cc.t))) {
		a = 1;
		if (G_is_null_value(&precCell, cc.t)) {
		    precCell = corrCell;
		}

		if (corrCell != precCell) {
		    if (albero == NULL) {
			cc.val.c = precCell;
			albero = avl_make(cc, totCorr);

			if (albero == NULL) {
			    G_fatal_error("avl_make error");
			    return RLI_ERRORE;
			}
			else
			    m++;
		    }
		    else {
			cc.val.c = precCell;
			ris = avl_add(&albero, cc, totCorr);
			switch (ris) {
			case AVL_ERR:
			    {
				G_fatal_error("avl_add error");
				return RLI_ERRORE;
			    }
			case AVL_ADD:
			    {
				m++;
				break;
			    }
			case AVL_PRES:
			    {
				break;
			    }
			default:
			    {
				G_fatal_error("avl_add unknown error");
				return RLI_ERRORE;
			    }
			}
		    }
		    totCorr = 1;
		}
		else {
		    totCorr++;
		}
		precCell = corrCell;
	    }

	}			/* end for */
    }				/* end for */


    /*last closing */
    if (a != 0) {
	if (albero == NULL) {
	    cc.val.c = precCell;
	    albero = avl_make(cc, totCorr);

	    if (albero == NULL) {
		G_fatal_error("avl_make error");
		return RLI_ERRORE;
	    }
	    m++;
	}
	else {
	    cc.val.c = precCell;
	    ris = avl_add(&albero, cc, totCorr);
	    switch (ris) {
	    case AVL_ERR:
		{
		    G_fatal_error("avl_add error");
		    return RLI_ERRORE;
		}
	    case AVL_ADD:
		{
		    m++;
		    break;
		}
	    case AVL_PRES:
		{
		    break;
		}
	    default:
		{
		    G_fatal_error("avl_add unknown error");
		    return RLI_ERRORE;
		}
	    }
	}
    }

    array = G_malloc(m * sizeof(AVL_tableRow));
    if (array == NULL) {
	G_fatal_error("malloc array failed");
	return RLI_ERRORE;
    }
    tot = avl_to_array(albero, zero, array);

    if (tot != m) {
	G_warning("avl_to_array unaspected value. the result could be wrong");
	return RLI_ERRORE;
    }

    array_copy = G_malloc(m * sizeof(AVL_tableRow));
    for (i = 0; i < m; i++) {
	cc_passage = array[i]->k;
	array_copy[i] = cc_passage.val.c;
    }

    qsort(array_copy, m, sizeof(int), cmp);

    for (i = 1; i < m; i++) {
	if (array_copy[i - 1] != array_copy[i]) {
	    totNumClass++;
	}
    }

    /* calculate summary */
    for (i = 0; i < m; i++) {
	t = (double)array[i]->tot;
	percentuale = (double)(t / area);
	logaritmo = (double)log(percentuale);
	somma = somma + (percentuale * logaritmo);
    }

    logTotNumClass = (double)log(totNumClass);
    /*if a is 0, that is all cell are null, i put index=-1 */
    if (a != 0) {
	indice = ((-1) * somma) / logTotNumClass;
    }
    else {
	indice = (double)(-1);
    }

    if (isnan(indice) || isinf(indice)) {
	indice = -1;
    }
    G_debug(1, "somma: %g, number of logTotNumClass: %g, valore finale: %g",
	    somma, logTotNumClass, indice);

    *result = indice;

    G_free(array);
    if (masked) {
	G_free(mask_buf);
    }

    return RLI_OK;
}



int calculateD(int fd, area_des ad, double *result)
{

    DCELL *buf;
    DCELL corrCell;
    DCELL precCell;

    int i, j;
    int mask_fd = -1, *mask_buf;
    int ris = 0;
    int masked = FALSE;

    int a = 0;			/* a=0 if all cells are null */
    long m = 0;
    long tot = 0;
    long zero = 0;
    long totCorr = 0;
    double indice = 0;
    double somma = 0;
    double percentuale = 0;
    double area = 0;
    double t;
    double logaritmo;

    /*add to compute the number of class */
    long *array_copy;
    int totNumClass = 1;
    double logTotNumClass;

    avl_tree albero = NULL;
    AVL_table *array;
    generic_cell cc;
    generic_cell cc_passage;	/*add to compute the number of class */

    cc.t = DCELL_TYPE;

    /* open mask if needed */
    if (ad->mask == 1) {
	if ((mask_fd = open(ad->mask_name, O_RDONLY, 0755)) < 0)
	    return RLI_ERRORE;
	mask_buf = G_malloc(ad->cl * sizeof(int));
	if (mask_buf == NULL) {
	    G_fatal_error("malloc mask_buf failed");
	    return RLI_ERRORE;
	}
	masked = TRUE;
    }


    G_set_d_null_value(&precCell, 1);

    /*for each row */
    for (j = 0; j < ad->rl; j++) {
	buf = RLI_get_dcell_raster_row(fd, j + ad->y, ad);


	if (masked) {
	    if (read(mask_fd, mask_buf, (ad->cl * sizeof(int))) < 0) {
		G_fatal_error("mask read failed");
		return RLI_ERRORE;
	    }
	}


	for (i = 0; i < ad->cl; i++) {	/* for each dcell in the row */
	    area++;
	    corrCell = buf[i + ad->x];

	    if (masked && mask_buf[i + ad->x] == 0) {
		G_set_d_null_value(&corrCell, 1);
		area--;
	    }

	    if (!(G_is_null_value(&corrCell, DCELL_TYPE))) {
		a = 1;
		if (G_is_null_value(&precCell, DCELL_TYPE)) {
		    precCell = corrCell;
		}
		if (corrCell != precCell) {
		    if (albero == NULL) {
			cc.val.dc = precCell;
			albero = avl_make(cc, totCorr);
			if (albero == NULL) {
			    G_fatal_error("avl_make error");
			    return RLI_ERRORE;
			}
			m++;
		    }
		    else {
			cc.val.dc = precCell;
			ris = avl_add(&albero, cc, totCorr);
			switch (ris) {
			case AVL_ERR:
			    {
				G_fatal_error("avl_add error");
				return RLI_ERRORE;
			    }
			case AVL_ADD:
			    {
				m++;
				break;
			    }
			case AVL_PRES:
			    {
				break;
			    }
			default:
			    {
				G_fatal_error("avl_add unknown error");
				return RLI_ERRORE;
			    }
			}
		    }
		    totCorr = 1;
		}
		else {
		    totCorr++;
		}
		precCell = corrCell;
	    }

	}			/*close for */
    }				/*close for */

    if (masked) {
	G_free(mask_buf);
    }

    /*last closing */
    if (a != 0) {
	if (albero == NULL) {
	    cc.val.dc = precCell;
	    albero = avl_make(cc, totCorr);
	    if (albero == NULL) {
		G_fatal_error("avl_make error");
		return RLI_ERRORE;
	    }
	    m++;
	}
	else {
	    cc.val.fc = precCell;
	    ris = avl_add(&albero, cc, totCorr);
	    switch (ris) {
	    case AVL_ERR:
		{
		    G_fatal_error("avl_add error");
		    return RLI_ERRORE;
		}
	    case AVL_ADD:
		{
		    m++;
		    break;
		}
	    case AVL_PRES:
		{
		    break;
		}
	    default:
		{
		    G_fatal_error("avl_add unknown error");
		    return RLI_ERRORE;
		}
	    }
	}


	array = G_malloc(m * sizeof(AVL_tableRow));
	if (array == NULL) {
	    G_fatal_error("malloc array failed");
	    return RLI_ERRORE;
	}
	tot = avl_to_array(albero, zero, array);

	if (tot != m) {
	    G_warning
		("avl_to_array unaspected value. the result could be wrong");
	    return RLI_ERRORE;
	}

	array_copy = G_malloc(m * sizeof(AVL_tableRow));

	for (i = 0; i < m; i++) {
	    cc_passage = array[i]->k;
	    array_copy[i] = cc_passage.val.c;
	}

	qsort(array_copy, m, sizeof(int), cmp);

	for (i = 1; i < m; i++) {
	    if (array_copy[i - 1] != array_copy[i]) {
		totNumClass++;
	    }
	}


	/* calculate summary */
	for (i = 0; i < m; i++) {
	    t = (double)array[i]->tot;
	    percentuale = (double)(t / area);
	    logaritmo = (double)log(percentuale);
	    somma = somma + (percentuale * logaritmo);
	}

	G_free(array);

	logTotNumClass = (double)log(totNumClass);

	indice = (-1) * somma / logTotNumClass;
    }
    else {
	/*if a is 0, that is all cell are null, i put index=-1 */
	indice = ((double)(-1));
    }

    if (isnan(indice) || isinf(indice)) {
	indice = -1;
    }
    G_debug(1, "somma: %g, number of logTotNumClass: %g, valore finale: %g",
	    somma, logTotNumClass, indice);

    *result = indice;

    return RLI_OK;
}


int calculateF(int fd, area_des ad, double *result)
{

    FCELL *buf;
    FCELL corrCell;
    FCELL precCell;

    int i, j;
    int mask_fd = -1, *mask_buf;
    int ris = 0;
    int masked = FALSE;
    int a = 0;			/* a=0 if all cells are null */
    long m = 0;
    long tot = 0;
    long zero = 0;
    long totCorr = 0;
    double indice = 0;
    double somma = 0;
    double percentuale = 0;
    double area = 0;
    double t;
    double logaritmo;
    long *array_copy;
    int totNumClass = 1;
    double logTotNumClass;

    avl_tree albero = NULL;
    AVL_table *array;
    generic_cell cc;
    generic_cell cc_passage;

    cc.t = FCELL_TYPE;

    /* open mask if needed */
    if (ad->mask == 1) {
	if ((mask_fd = open(ad->mask_name, O_RDONLY, 0755)) < 0)
	    return RLI_ERRORE;
	mask_buf = G_malloc(ad->cl * sizeof(int));
	if (mask_buf == NULL) {
	    G_fatal_error("malloc mask_buf failed");
	    return RLI_ERRORE;
	}
	masked = TRUE;
    }


    G_set_f_null_value(&precCell, 1);


    /*for each row */
    for (j = 0; j < ad->rl; j++) {
	buf = RLI_get_fcell_raster_row(fd, j + ad->y, ad);


	if (masked) {
	    if (read(mask_fd, mask_buf, (ad->cl * sizeof(int))) < 0) {
		G_fatal_error("mask read failed");
		return RLI_ERRORE;
	    }
	}

	for (i = 0; i < ad->cl; i++) {	/* for each fcell in the row */
	    area++;
	    corrCell = buf[i + ad->x];
	    if (masked && mask_buf[i + ad->x] == 0) {
		G_set_f_null_value(&corrCell, 1);
		area--;
	    }

	    if (!(G_is_null_value(&corrCell, cc.t))) {
		a = 1;
		if (G_is_null_value(&precCell, cc.t)) {
		    precCell = corrCell;
		}
		if (corrCell != precCell) {
		    if (albero == NULL) {
			cc.val.fc = precCell;
			albero = avl_make(cc, totCorr);
			if (albero == NULL) {
			    G_fatal_error("avl_make error");
			    return RLI_ERRORE;
			}
			m++;
		    }
		    else {
			cc.val.fc = precCell;
			ris = avl_add(&albero, cc, totCorr);
			switch (ris) {
			case AVL_ERR:
			    {
				G_fatal_error("avl_add error");
				return RLI_ERRORE;
			    }
			case AVL_ADD:
			    {
				m++;
				break;
			    }
			case AVL_PRES:
			    {
				break;
			    }
			default:
			    {
				G_fatal_error("avl_add unknown error");
				return RLI_ERRORE;
			    }
			}
		    }
		    totCorr = 1;
		}
		else {
		    totCorr++;
		}

		precCell = corrCell;
	    }

	}
    }

    /*last closing */
    if (a != 0) {
	if (albero == NULL) {
	    cc.val.fc = precCell;
	    albero = avl_make(cc, totCorr);
	    if (albero == NULL) {
		G_fatal_error("avl_make error");
		return RLI_ERRORE;
	    }
	    m++;
	}
	else {
	    cc.val.fc = precCell;
	    ris = avl_add(&albero, cc, totCorr);
	    switch (ris) {
	    case AVL_ERR:
		{
		    G_fatal_error("avl_add error");
		    return RLI_ERRORE;
		}
	    case AVL_ADD:
		{
		    m++;
		    break;
		}
	    case AVL_PRES:
		{
		    break;
		}
	    default:
		{
		    G_fatal_error("avl_add unknown error");
		    return RLI_ERRORE;
		}
	    }
	}
    }

    array = G_malloc(m * sizeof(AVL_tableRow));
    if (array == NULL) {
	G_fatal_error("malloc array failed");
	return RLI_ERRORE;
    }
    tot = avl_to_array(albero, zero, array);

    if (tot != m) {
	G_warning("avl_to_array unaspected value. the result could be wrong");
	return RLI_ERRORE;
    }

    array_copy = G_malloc(m * sizeof(AVL_tableRow));

    for (i = 0; i < m; i++) {
	cc_passage = array[i]->k;
	array_copy[i] = cc_passage.val.c;
    }

    qsort(array_copy, m, sizeof(int), cmp);

    for (i = 1; i < m; i++) {
	if (array_copy[i - 1] != array_copy[i]) {
	    totNumClass++;
	}
    }

    /* calculate summary */
    for (i = 0; i < m; i++) {
	t = (double)array[i]->tot;
	percentuale = (double)(t / area);
	logaritmo = (double)log(percentuale);
	somma = somma + (percentuale * logaritmo);
    }

    logTotNumClass = (double)log(totNumClass);


    /*if a is 0, that is all cell are null, i put index=-1 */
    if (a != 0) {
	indice = ((-1) * somma) / logTotNumClass;
    }
    else {
	indice = (double)(-1);
    }

    if (isnan(indice) || isinf(indice)) {
	indice = -1;
    }
    G_debug(1, "somma: %g, number of logTotNumClass: %g, valore finale: %g",
	    somma, logTotNumClass, indice);

    *result = indice;

    G_free(array);
    if (masked) {
	G_free(mask_buf);
    }
    return RLI_OK;
}
