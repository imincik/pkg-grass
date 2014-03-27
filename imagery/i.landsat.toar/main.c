
/****************************************************************************
 *
 * MODULE:       i.landsat.toar
 *
 * AUTHOR(S):    E. Jorge Tizado - ej.tizado@unileon.es
 *               Hamish Bowman (small grassification cleanups)
 *               Yann Chemin (v7 + L5TM _MTL.txt support) [removed after update]
 *
 * PURPOSE:      Calculate TOA Radiance or Reflectance and Kinetic Temperature
 *               for Landsat 1/2/3/4/5 MS, 4/5 TM, 7 ETM+, and 8 OLI/TIRS
 *
 * COPYRIGHT:    (C) 2006-2013 by the GRASS Development Team
 *
 *               This program is free software under the GNU General
 *               Public License (>=v2). Read the file COPYING that
 *               comes with GRASS for details.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <grass/gis.h>
#include <grass/glocale.h>

#include "local_proto.h"

int main(int argc, char *argv[])
{
    struct History history;
    struct GModule *module;

    struct Cell_head cellhd;
    char *mapset;

    void *inrast, *outrast;
    int infd, outfd;
    void *ptr;
    int nrows, ncols, row, col;

    RASTER_MAP_TYPE in_data_type;

    struct Option *input_prefix, *output_prefix, *metfn, *sensor, *adate,
	*pdate, *elev, *bgain, *metho, *perc, *dark, *atmo;
    char *inputname, *met, *outputname, *sensorname;
    struct Flag *frad, *named;

    lsat_data lsat;
    char band_in[GNAME_MAX], band_out[GNAME_MAX];
    int i, j, q, method, pixel, dn_dark[MAX_BANDS], dn_mode[MAX_BANDS];
    double qcal, rad, ref, percent, ref_mode, rayleigh;

    struct Colors colors;
    struct FPRange range;
    double min, max;
    unsigned long hist[256], h_max;

    /* initialize GIS environment */
    G_gisinit(argv[0]);

    /* initialize module */
    module = G_define_module();
    module->description =
	_("Calculates top-of-atmosphere radiance or reflectance and temperature for Landsat MSS/TM/ETM+.");
    module->keywords =
	_("imagery, landsat, top-of-atmosphere reflectance, dos-type simple atmospheric correction");

    /* It defines the different parameters */
    input_prefix = G_define_option();
    input_prefix->key = "input_prefix";
    input_prefix->label = _("Base name of input raster bands");
    input_prefix->description = _("Example: 'B.' for B.1, B.2, ...");
    input_prefix->type = TYPE_STRING;
    input_prefix->required = YES;

    output_prefix = G_define_option();
    output_prefix->key = "output_prefix";
    output_prefix->label = _("Prefix for output raster maps");
    output_prefix->description =
	_("Example: 'B.toar.' generates B.toar.1, B.toar.2, ...");
    output_prefix->type = TYPE_STRING;
    output_prefix->required = YES;

    metfn = G_define_standard_option(G_OPT_F_INPUT);
    metfn->key = "metfile";
    metfn->required = NO;
    metfn->description = _("Name of Landsat metadata file (.met or MTL.txt)");
    metfn->guisection = _("Metadata");

    sensor = G_define_option();
    sensor->key = "sensor";
    sensor->type = TYPE_STRING;
    sensor->label = _("Spacecraft sensor");
    sensor->description =
	_("Required only if 'metfile' not given (recommended for sanity)");
    sensor->options = "mss1,mss2,mss3,mss4,mss5,tm4,tm5,tm7,ot8";
    sensor->descriptions =
	_("mss1;Landsat_1 MSS;"
	  "mss2;Landsat_2 MSS;"
	  "mss3;Landsat_3 MSS;"
	  "mss4;Landsat_4 MSS;"
	  "mss5;Landsat_5 MSS;"
	  "tm4;Landsat_4 TM;"
	  "tm5;Landsat_5 TM;"
          "tm7;Landsat_7 ETM+;"
          "ot8;Landsat_8 OLI/TIRS");
    sensor->required = NO;	/* perhaps YES for clarity */
    sensor->guisection = _("Metadata");

    metho = G_define_option();
    metho->key = "method";
    metho->type = TYPE_STRING;
    metho->required = NO;
    metho->options = "uncorrected,corrected,dos1,dos2,dos2b,dos3,dos4";
    metho->label = _("Atmospheric correction method");
    metho->description = _("Atmospheric correction method");
    metho->answer = "uncorrected";
    metho->guisection = _("Metadata");

    adate = G_define_option();
    adate->key = "date";
    adate->type = TYPE_STRING;
    adate->required = NO;
    adate->key_desc = "yyyy-mm-dd";
    adate->label = _("Image acquisition date (yyyy-mm-dd)");
    adate->description = _("Required only if 'metfile' not given");
    adate->guisection = _("Metadata");

    elev = G_define_option();
    elev->key = "sun_elevation";
    elev->type = TYPE_DOUBLE;
    elev->required = NO;
    elev->label = _("Sun elevation in degrees");
    elev->description = _("Required only if 'metfile' not given");
    elev->guisection = _("Metadata");

    pdate = G_define_option();
    pdate->key = "product_date";
    pdate->type = TYPE_STRING;
    pdate->required = NO;
    pdate->key_desc = "yyyy-mm-dd";
    pdate->label = _("Image creation date (yyyy-mm-dd)");
    pdate->description = _("Required only if 'metfile' not given");
    pdate->guisection = _("Metadata");

    bgain = G_define_option();
    bgain->key = "gain";
    bgain->type = TYPE_STRING;
    bgain->required = NO;
    bgain->label = _("Gain (H/L) of all Landsat ETM+ bands (1-5,61,62,7,8)");
    bgain->description = _("Required only if 'metfile' not given");
    bgain->guisection = _("Settings");

    perc = G_define_option();
    perc->key = "percent";
    perc->type = TYPE_DOUBLE;
    perc->required = NO;
    perc->label = _("Percent of solar radiance in path radiance");
    perc->description = _("Required only if 'method' is any DOS");
    perc->answer = "0.01";
    perc->guisection = _("Settings");

    dark = G_define_option();
    dark->key = "pixel";
    dark->type = TYPE_INTEGER;
    dark->required = NO;
    dark->label =
	_("Minimum pixels to consider digital number as dark object");
    dark->description = _("Required only if 'method' is any DOS");
    dark->answer = "1000";
    dark->guisection = _("Settings");

    atmo = G_define_option();
    atmo->key = "rayleigh";
    atmo->type = TYPE_DOUBLE;
    atmo->required = NO;
    atmo->label = _("Rayleigh atmosphere (diffuse sky irradiance)");	/* scattering coefficient? */
    atmo->description = _("Required only if 'method' is DOS3");
    atmo->answer = "0.0";
    atmo->guisection = _("Settings");

    /* define the different flags */
    frad = G_define_flag();
    frad->key = 'r';
    frad->description =
	_("Output at-sensor radiance instead of reflectance for all bands");

    named = G_define_flag();
    named->key = 'n';
    named->description =
	_("Input raster maps use as extension the number of the band instead the code");

    /* options and afters parser */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);


    /*****************************************
     * ---------- START --------------------
     * Stores options and flag to variables
     *****************************************/
    met = metfn->answer;
    inputname = input_prefix->answer;
    outputname = output_prefix->answer;
    sensorname = sensor->answer ? sensor->answer : "";

    G_zero(&lsat, sizeof(lsat));

    if (adate->answer != NULL) {
	strncpy(lsat.date, adate->answer, 11);
	lsat.date[10] = '\0';
	if (strlen(lsat.date) != 10)
	    G_fatal_error(_("Illegal date format: [%s] (yyyy-mm-dd)"),
			  lsat.date);
    }

    if (pdate->answer != NULL) {
	strncpy(lsat.creation, pdate->answer, 11);
	lsat.creation[10] = '\0';
	if (strlen(lsat.creation) != 10)
	    G_fatal_error(_("Illegal date format: [%s] (yyyy-mm-dd)"),
			  lsat.creation);
    }

    lsat.sun_elev = elev->answer == NULL ? 0. : atof(elev->answer);

    percent = atof(perc->answer);
    pixel = atoi(dark->answer);
    rayleigh = atof(atmo->answer);

    /* Data from metadata file */
    /* Unnecessary because G_zero filled, but for sanity */
    lsat.flag = NOMETADATAFILE;
    if (met != NULL) {
	lsat.flag = METADATAFILE;
	lsat_metadata(met, &lsat);
	G_debug(1, "lsat.number = %d, lsat.sensor = [%s]", lsat.number,
		lsat.sensor);

	if (!lsat.sensor || lsat.number > 8 || lsat.number < 1)
	    G_fatal_error(_("Failed to identify satellite"));

	G_debug(1, "Landsat-%d %s with data set in met file [%s]",
		lsat.number, lsat.sensor, met);

	/* Overwrite solar elevation of metadata file */
	if (elev->answer != NULL)
	    lsat.sun_elev = atof(elev->answer);
    }
    /* Data from date and solar elevation */
    else if (adate->answer == NULL || elev->answer == NULL) {
	G_fatal_error(_("Lacking '%s' and/or '%s' for this satellite"),
		      adate->key, elev->key);
    }
    else {
	/* Need gain */
	if (strcmp(sensorname, "tm7") == 0) {
	    if (bgain->answer == NULL || strlen(bgain->answer) != 9)
		G_fatal_error(_("Landsat-7 requires band gain with 9 (H/L) data"));
	    set_ETM(&lsat, bgain->answer);
	}
	/* Not need gain */
	else if (strcmp(sensorname, "ot8") == 0)
	    set_LDCM(&lsat);
	else if (strcmp(sensorname, "tm5") == 0)
	    set_TM5(&lsat);
	else if (strcmp(sensorname, "tm4") == 0)
	    set_TM4(&lsat);
	else if (strcmp(sensorname, "mss5") == 0)
	    set_MSS5(&lsat);
	else if (strcmp(sensorname, "mss4") == 0)
	    set_MSS4(&lsat);
	else if (strcmp(sensorname, "mss3") == 0)
	    set_MSS3(&lsat);
	else if (strcmp(sensorname, "mss2") == 0)
	    set_MSS2(&lsat);
	else if (strcmp(sensorname, "mss1") == 0)
	    set_MSS1(&lsat);
	else
	    G_fatal_error(_("Unknown satellite type (defined by '%s')"),
			  sensor->key);
    }

	/*****************************************
	* ------------ PREPARATION --------------
	*****************************************/
    if (strcasecmp(metho->answer, "corrected") == 0)
	method = CORRECTED;
    else if (strcasecmp(metho->answer, "dos1") == 0)
	method = DOS1;
    else if (strcasecmp(metho->answer, "dos2") == 0)
	method = DOS2;
    else if (strcasecmp(metho->answer, "dos2b") == 0)
	method = DOS2b;
    else if (strcasecmp(metho->answer, "dos3") == 0)
	method = DOS3;
    else if (strcasecmp(metho->answer, "dos4") == 0)
	method = DOS4;
    else
	method = UNCORRECTED;

    /*
       if (metho->answer[3] == 'r')            method = CORRECTED;
       else if (metho->answer[3] == '1')       method = DOS1;
       else if (metho->answer[3] == '2')       method = (metho->answer[4] == '\0') ? DOS2 : DOS2b;
       else if (metho->answer[3] == '3')       method = DOS3;
       else if (metho->answer[3] == '4')       method = DOS4;
       else method = UNCORRECTED;
     */

    for (i = 0; i < lsat.bands; i++) {
	dn_mode[i] = 0;
	dn_dark[i] = (int)lsat.band[i].qcalmin;
	/* Calculate dark pixel */
	if (method > DOS && !lsat.band[i].thermal) {
	    for (j = 0; j < 256; j++)
		hist[j] = 0L;

	    sprintf(band_in, "%s%d", inputname, lsat.band[i].code);
	    mapset = G_find_cell2(band_in, "");
	    if (mapset == NULL) {
		G_warning(_("Raster map <%s> not found"), band_in);
		continue;
	    }
	    if ((infd = G_open_cell_old(band_in, "")) < 0)
		G_fatal_error(_("Unable to open raster map <%s>"), band_in);
	    if (G_get_cellhd(band_in, mapset, &cellhd) < 0)
		G_fatal_error(_("Unable to read header of raster map <%s>"),
			      band_in);
	    G_set_window(&cellhd);

	    in_data_type = G_raster_map_type(band_in, mapset);
	    inrast = G_allocate_raster_buf(in_data_type);

	    nrows = G_window_rows();
	    ncols = G_window_cols();

	    G_message("Calculating dark pixel of <%s>... ", band_in);
	    for (row = 0; row < nrows; row++) {
		G_get_raster_row(infd, inrast, row, in_data_type);
		for (col = 0; col < ncols; col++) {
		    switch (in_data_type) {
		    case CELL_TYPE:
			ptr = (void *)((CELL *) inrast + col);
			q = (int)*((CELL *) ptr);
			break;
		    case FCELL_TYPE:
			ptr = (void *)((FCELL *) inrast + col);
			q = (int)*((FCELL *) ptr);
			break;
		    case DCELL_TYPE:
			ptr = (void *)((DCELL *) inrast + col);
			q = (int)*((DCELL *) ptr);
			break;
		    }
		    if (!G_is_null_value(ptr, in_data_type) &&
			q >= lsat.band[i].qcalmin && q < 256)
			hist[q]++;
		}
	    }
	    /* DN of dark object */
	    for (j = lsat.band[i].qcalmin; j < 256; j++) {
		if (hist[j] >= (unsigned int)pixel) {
		    dn_dark[i] = j;
		    break;
		}
	    }
	    /* Mode of DN */
	    h_max = 0L;
	    for (j = lsat.band[i].qcalmin; j < 241; j++) {	/* Exclude ptentially saturated < 240 */
		/* G_debug(5, "%d-%ld", j, hist[j]); */
		if (hist[j] > h_max) {
		    h_max = hist[j];
		    dn_mode[i] = j;
		}
	    }
	    G_verbose_message("... DN = %.2d [%lu] : mode %.2d [%lu] %s",
			      dn_dark[i], hist[dn_dark[i]],
			      dn_mode[i], hist[dn_mode[i]],
			      hist[255] >
			      hist[dn_mode[i]] ? ", excluding DN > 241" : "");

	    G_free(inrast);
	    G_close_cell(infd);
	}
	/* Calculate transformation constants */
	lsat_bandctes(&lsat, i, method, percent, dn_dark[i], rayleigh);
    }

    /* unnecessary or necessary with more checking as acquisition date,...
       if (strlen(lsat.creation) == 0)
       G_fatal_error(_("Unknown production date (defined by '%s')"), pdate->key);
     */

    if (G_verbose() > G_verbose_std()) {
	fprintf(stderr, " LANDSAT: %d SENSOR: %s\n", lsat.number,
		lsat.sensor);
	fprintf(stderr, " ACQUISITION DATE %s [production date %s]\n",
		lsat.date, lsat.creation);
	fprintf(stderr, "   earth-sun distance    = %.8lf\n", lsat.dist_es);
	fprintf(stderr, "   solar elevation angle = %.8lf\n", lsat.sun_elev);
	fprintf(stderr, "   Atmospheric correction: %s\n",
		(method == CORRECTED ? "CORRECTED"
		 : (method == UNCORRECTED ? "UNCORRECTED" : metho->answer)));
	if (method > DOS) {
	    fprintf(stderr,
		    "   percent of solar irradiance in path radiance = %.4lf\n",
		    percent);
	}
	for (i = 0; i < lsat.bands; i++) {
	    fprintf(stderr, "-------------------\n");
	    fprintf(stderr, " BAND %d %s(code %d)\n",
		    lsat.band[i].number,
		    (lsat.band[i].thermal ? "thermal " : ""),
		    lsat.band[i].code);
	    fprintf(stderr,
		    "   calibrated digital number (DN): %.1lf to %.1lf\n",
		    lsat.band[i].qcalmin, lsat.band[i].qcalmax);
	    fprintf(stderr, "   calibration constants (L): %.3lf to %.3lf\n",
		    lsat.band[i].lmin, lsat.band[i].lmax);
	    fprintf(stderr, "   at-%s radiance = %.8lf * DN + %.3lf\n",
		    (method > DOS ? "surface" : "sensor"), lsat.band[i].gain,
		    lsat.band[i].bias);
	    if (lsat.band[i].thermal) {
		fprintf(stderr,
			"   at-sensor temperature = %.5lf / log[(%.5lf / radiance) + 1.0]\n",
			lsat.band[i].K2, lsat.band[i].K1);
	    }
	    else {
		fprintf(stderr,
			"   mean solar exoatmospheric irradiance (ESUN): %.3lf\n",
			lsat.band[i].esun);
		fprintf(stderr, "   at-%s reflectance = radiance / %.5lf\n",
			(method > DOS ? "surface" : "sensor"),
			lsat.band[i].K1);
		if (method > DOS) {
		    fprintf(stderr,
			    "   the darkness DN with a least %d pixels is %d\n",
			    pixel, dn_dark[i]);
		    fprintf(stderr, "   the DN mode is %d\n", dn_mode[i]);
		}
	    }
	}
	fprintf(stderr, "-------------------\n");
	fflush(stderr);
    }

    /*****************************************
    * ------------ CALCULUS -----------------
    *****************************************/

    G_message(_("Calculating..."));
    for (i = 0; i < lsat.bands; i++) {
	sprintf(band_in, "%s%d", inputname,
		(named->answer ? lsat.band[i].number : lsat.band[i].code));
	sprintf(band_out, "%s%d", outputname, lsat.band[i].code);

	mapset = G_find_cell2(band_in, "");
	if (mapset == NULL) {
	    G_warning(_("Raster map <%s> not found"), band_in);
	    continue;
	}

	if ((infd = G_open_cell_old(band_in, mapset)) < 0)
	    G_fatal_error(_("Unable to open raster map <%s>"), band_in);
	in_data_type = G_raster_map_type(band_in, mapset);
	if (G_get_cellhd(band_in, mapset, &cellhd) < 0)
	    G_fatal_error(_("Unable to read header of raster map <%s>"),
			  band_in);

	/* set same size as original band raster */
	G_set_window(&cellhd);

	/* controlling, if we can write the raster */
	if (G_legal_filename(band_out) < 0)
	    G_fatal_error(_("<%s> is an illegal file name"), band_out);

	if ((outfd = G_open_raster_new(band_out, DCELL_TYPE)) < 0)
	    G_fatal_error(_("Unable to create raster map <%s>"), band_out);

	/* Allocate input and output buffer */
	inrast = G_allocate_raster_buf(in_data_type);
	outrast = G_allocate_raster_buf(DCELL_TYPE);

	nrows = G_window_rows();
	ncols = G_window_cols();
	/* ================================================================= */
	G_important_message(_("Writing %s of <%s> to <%s>..."),
			    (frad->answer ? _("radiance")
			     : (lsat.
				band[i].thermal) ? _("temperature") :
			     _("reflectance")), band_in, band_out);
	for (row = 0; row < nrows; row++) {
	    G_percent(row, nrows, 2);

	    G_get_raster_row(infd, inrast, row, in_data_type);
	    for (col = 0; col < ncols; col++) {
		switch (in_data_type) {
		case CELL_TYPE:
		    ptr = (void *)((CELL *) inrast + col);
		    qcal = (double)((CELL *) inrast)[col];
		    break;
		case FCELL_TYPE:
		    ptr = (void *)((FCELL *) inrast + col);
		    qcal = (double)((FCELL *) inrast)[col];
		    break;
		case DCELL_TYPE:
		    ptr = (void *)((DCELL *) inrast + col);
		    qcal = (double)((DCELL *) inrast)[col];
		    break;
		}
		if (G_is_null_value(ptr, in_data_type) ||
		    qcal < lsat.band[i].qcalmin) {
		    G_set_d_null_value((DCELL *) outrast + col, 1);
		}
		else {
		    rad = lsat_qcal2rad(qcal, &lsat.band[i]);
		    if (frad->answer) {
			ref = rad;
		    }
		    else {
			if (lsat.band[i].thermal) {
			    ref = lsat_rad2temp(rad, &lsat.band[i]);
			}
			else {
			    ref = lsat_rad2ref(rad, &lsat.band[i]);
			    if (ref < 0. && method > DOS)
				ref = 0.;
			}
		    }
		    ((DCELL *) outrast)[col] = ref;
		}
	    }
	    G_put_raster_row(outfd, outrast, DCELL_TYPE);
	}
	G_percent(1, 1, 1);

	ref_mode = 0.;
	if (method > DOS && !lsat.band[i].thermal) {
	    ref_mode = lsat_qcal2rad(dn_mode[i], &lsat.band[i]);
	    ref_mode = lsat_rad2ref(ref_mode, &lsat.band[i]);
	}

	/* ================================================================= */

	G_free(inrast);
	G_close_cell(infd);
	G_free(outrast);
	G_close_cell(outfd);

	/* needed ?
	   if (out_type != CELL_TYPE)
	   G_quantize_fp_map_range(band_out, G_mapset(), 0., 360., 0, 360);
	 */
	/* set grey255 colortable */
	G_init_colors(&colors);
	G_read_fp_range(band_out, G_mapset(), &range);
	G_get_fp_range_min_max(&range, &min, &max);
	G_make_grey_scale_fp_colors(&colors, min, max);
	G_write_colors(band_out, G_mapset(), &colors);

	/* Initialize the 'hist' structure with basic info */
	G_short_history(band_out, "raster", &history);
	sprintf(history.edhist[0], " %s of Landsat-%d %s (method %s)",
		(frad->
		 answer ? "Radiance" : (lsat.band[i].
					thermal ? "Temperature" :
					"Reflectance")), lsat.number,
		lsat.sensor, metho->answer);
	sprintf(history.edhist[1],
		"----------------------------------------------------------------");
	sprintf(history.edhist[2],
		" Acquisition date ...................... %s", lsat.date);
	sprintf(history.edhist[3],
		" Production date ....................... %s\n",
		lsat.creation);
	sprintf(history.edhist[4],
		" Earth-sun distance (d) ................ %.8lf",
		lsat.dist_es);
	sprintf(history.edhist[5],
		" Digital number (DN) range ............. %.0lf to %.0lf",
		lsat.band[i].qcalmin, lsat.band[i].qcalmax);
	sprintf(history.edhist[6],
		" Calibration constants (Lmin to Lmax) .. %+.3lf to %+.3lf",
		lsat.band[i].lmin, lsat.band[i].lmax);
	sprintf(history.edhist[7],
		" DN to Radiance (gain and bias) ........ %+.5lf and %+.5lf",
		lsat.band[i].gain, lsat.band[i].bias);
	if (lsat.band[i].thermal) {
	    sprintf(history.edhist[8],
		    " Temperature (K1 and K2) ............... %.3lf and %.3lf",
		    lsat.band[i].K1, lsat.band[i].K2);
	    history.edlinecnt = 9;
	}
	else {
	    sprintf(history.edhist[8],
		    " Mean solar irradiance (ESUN) .......... %.3lf",
		    lsat.band[i].esun);
	    sprintf(history.edhist[9],
		    " Reflectance = Radiance divided by ..... %.5lf",
		    lsat.band[i].K1);
	    history.edlinecnt = 10;
	    if (method > DOS) {
		sprintf(history.edhist[10], " ");
		sprintf(history.edhist[11],
			" Dark object (%4d pixels) DN = ........ %d", pixel,
			dn_dark[i]);
		sprintf(history.edhist[12],
			" Mode in reflectance histogram ......... %.5lf",
			ref_mode);
		history.edlinecnt = 13;
	    }
	}
	sprintf(history.edhist[history.edlinecnt],
		"-----------------------------------------------------------------");
	history.edlinecnt++;

	G_command_history(&history);
	G_write_history(band_out, &history);

	if (lsat.band[i].thermal)
	    G_write_raster_units(band_out, "Kelvin");
	else if (frad->answer)
	    G_write_raster_units(band_out, "W/(m^2 sr um)");
	else
	    G_write_raster_units(band_out, "unitless");

	/* set raster timestamp from acq date? (see r.timestamp module) */
    }

    exit(EXIT_SUCCESS);
}
