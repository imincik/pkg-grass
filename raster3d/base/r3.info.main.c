
/***************************************************************************
*
* MODULE:       r3.info
*
* AUTHOR(S):    Roman Waupotitsch, Michael Shapiro, Helena Mitasova, Bill Brown,
*               Lubos Mitas, Jaro Hofierka
*
* PURPOSE:      Outputs basic information about a user-specified 3D raster map layer.
*
* COPYRIGHT:    (C) 2005 by the GRASS Development Team
*
*               This program is free software under the GNU General Public
*               License (>=v2). Read the file COPYING that comes with GRASS
*               for details.
*
*****************************************************************************/

/* \todo
 *    History support still not full implemented.
 *    Only parts of the timestep functionality are implemented, the timzone is missed ;).
 */

/*local prototype */
int format_double(double value, char *buf);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/G3d.h>
#include <grass/glocale.h>

#define printline(x) fprintf (out," | %-74.74s |\n",x)
#define divider(x) \
    fprintf (out," %c",x);\
    for (i = 0; i < 76; i++)\
        fprintf(out,"-");\
    fprintf (out,"%c\n",x)

#define TMP_LENGTH 100

static char *name;

/**************************************************************************/
int main(int argc, char *argv[])
{
    char *mapset;
    char *line = NULL;
    char tmp1[TMP_LENGTH], tmp2[TMP_LENGTH], tmp3[TMP_LENGTH];
    char timebuff[256];
    int i;
    FILE *out;
    G3D_Region cellhd;
    void *g3map;
    struct Categories cats;
    struct History hist;
    struct TimeStamp ts;
    int head_ok;
    int cats_ok;
    int hist_ok;
    int time_ok = 0, first_time_ok = 0, second_time_ok = 0;
    struct Option *opt1;
    struct Flag *rflag;
    struct Flag *sflag;
    struct Flag *tflag;
    struct Flag *timestampflag;
    struct Flag *gflag;
    struct Flag *hflag;
    int data_type;

    struct GModule *module;
    double dmin, dmax;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("raster3d, voxel");
    module->description =
	_("Outputs basic information about a user-specified 3D raster map layer.");

    opt1 = G_define_option();
    opt1->key = "map";
    opt1->type = TYPE_STRING;
    opt1->required = YES;
    opt1->gisprompt = "old,grid3,3d raster";
    opt1->description = _("Name of input 3D raster map");

    rflag = G_define_flag();
    rflag->key = 'r';
    rflag->description = _("Print range only");

    sflag = G_define_flag();
    sflag->key = 's';
    sflag->description =
	_("Print 3D raster map resolution (NS-res, EW-res, TB-res) only");

    tflag = G_define_flag();
    tflag->key = 't';
    tflag->description = _("Print 3D raster map type (float/double) only");

    gflag = G_define_flag();
    gflag->key = 'g';
    gflag->description = _("Print 3D raster map region only");

    hflag = G_define_flag();
    hflag->key = 'h';
    hflag->description = _("Print 3D raster history instead of info");

    timestampflag = G_define_flag();
    timestampflag->key = 'p';
    timestampflag->description =
	_("Print 3D raster map timestamp (day.month.year hour:minute:seconds) only");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    name = G_store(opt1->answer);

    if ((mapset = G_find_grid3(name, "")) == NULL)
	G_fatal_error(_("3D Raster map <%s> not found"), name);

    /*We need to open the map */
    g3map =
	G3d_openCellOld(name, mapset, G3D_DEFAULT_WINDOW,
			G3D_TILE_SAME_AS_FILE, G3D_NO_CACHE);
    if (NULL == g3map)
	G_fatal_error(_("Unable to open 3D raster map <%s>"), name);

    /*Get the maptype */
    data_type = G3d_fileTypeMap(g3map);
    head_ok = G3d_readRegionMap(name, mapset, &cellhd) >= 0;
    hist_ok = G3d_readHistory(name, mapset, &hist) >= 0;
    cats_ok = G3d_readCats(name, mapset, &cats) >= 0;
    /*Check the Timestamp */
    time_ok = G_read_grid3_timestamp(name, mapset, &ts) > 0;

    /*Check for valid entries, show none if no entire available! */
    if (time_ok) {
	if (ts.count > 0)
	    first_time_ok = 1;
	if (ts.count > 1)
	    second_time_ok = 1;
    }

    out = stdout;

    /*Show the info if no flag is set */
    if (!rflag->answer && !sflag->answer && !tflag->answer && !gflag->answer
	&& !timestampflag->answer && !hflag->answer) {
	divider('+');

	if (G_asprintf
	    (&line, "Layer:    %-29.29s  Date: %s", name,
	     hist_ok ? hist.mapid : "??") > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));


	if (G_asprintf
	    (&line, "Mapset:   %-29.29s  Login of Creator: %s", mapset,
	     hist_ok ? hist.creator : "??") > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));


	if (G_asprintf(&line, "Location: %s", G_location()) > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));

	if (G_asprintf(&line, "DataBase: %s", G_gisdbase()) > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));

	if (G_asprintf
	    (&line, "Title:    %s ( %s )", cats_ok ? cats.title : "??",
	     hist_ok ? hist.title : "??") > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));

	/*This shows the TimeStamp */
	if (time_ok && (first_time_ok || second_time_ok)) {

	    G_format_timestamp(&ts, timebuff);

	    /*Create the r.info timestamp string */
	    if (G_asprintf(&line, "Timestamp: %s", timebuff) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));
	}
	else {
	    if (G_asprintf(&line, "Timestamp: none") > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));
	}


	divider('|');
	printline("");

	if (cats_ok) {
	    format_double((double)cats.num, tmp1);
	}

	if (G_asprintf
	    (&line, "  Type of Map:  %-20.20s Number of Categories: %-9s",
	     "3d cell", cats_ok ? tmp1 : "??") > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));

	if (G_asprintf(&line, "  Data Type:    %s",
		       (data_type == FCELL_TYPE ? "float" :
			(data_type == DCELL_TYPE ? "double" : "??"))) > 0)
	    printline(line);
	else
	    G_fatal_error(_("Cannot allocate memory for string"));


	if (head_ok) {
	    if (G_asprintf(&line, "  Rows:         %d", cellhd.rows) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    if (G_asprintf(&line, "  Columns:      %d", cellhd.cols) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    if (G_asprintf(&line, "  Depths:       %d", cellhd.depths) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    if (G_asprintf
		(&line, "  Total Cells:  %ld",
		 (long)cellhd.rows * cellhd.cols * cellhd.depths) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    if (G_asprintf
		(&line, "       Projection: %s (zone %d)",
		 G_database_projection_name(), G_zone()) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    G_format_northing(cellhd.north, tmp1, cellhd.proj);
	    G_format_northing(cellhd.south, tmp2, cellhd.proj);
	    G_format_resolution(cellhd.ns_res, tmp3, cellhd.proj);
	    if (G_asprintf
		(&line, "           N: %10s    S: %10s   Res: %5s", tmp1,
		 tmp2, tmp3) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    G_format_easting(cellhd.east, tmp1, cellhd.proj);
	    G_format_easting(cellhd.west, tmp2, cellhd.proj);
	    G_format_resolution(cellhd.ew_res, tmp3, cellhd.proj);
	    if (G_asprintf
		(&line, "           E: %10s    W: %10s   Res: %5s", tmp1,
		 tmp2, tmp3) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    format_double(cellhd.top, tmp1);
	    format_double(cellhd.bottom, tmp2);
	    format_double(cellhd.tb_res, tmp3);
	    if (G_asprintf
		(&line, "           T: %10s    B: %10s   Res: %5s", tmp1,
		 tmp2, tmp3) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));
	    if (0 == G3d_range_load(g3map))
		G_fatal_error(_("Unable to read range of 3D raster map <%s>"), name);

	    G3d_range_min_max(g3map, &dmin, &dmax);
	    format_double(dmin, tmp1);
	    format_double(dmax, tmp2);

	    if (G_asprintf
		(&line, "  Range of data:   min = %10s max = %10s", tmp1,
		 tmp2) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));
	}

	printline("");

	if (hist_ok) {
	    printline("  Data Source:");
	    if (G_asprintf(&line, "   %s", hist.datsrc_1) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    if (G_asprintf(&line, "   %s", hist.datsrc_2) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    printline("");

	    printline("  Data Description:");
	    if (G_asprintf(&line, "   %s", hist.keywrd) > 0)
		printline(line);
	    else
		G_fatal_error(_("Cannot allocate memory for string"));

	    printline("");
	    if (hist.edlinecnt) {
		printline("  Comments:  ");

		for (i = 0; i < hist.edlinecnt; i++)

	    /**************************************/
		{
		    if (G_asprintf(&line, "   %s", hist.edhist[i]) > 0)
			printline(line);
		    else
			G_fatal_error(_("Cannot allocate memory for string"));

		}
	    }

	    printline("");
	}

	divider('+');

	fprintf(out, "\n");
    }
    else {			/* Range */
	if (rflag->answer) {
	    if (0 == G3d_range_load(g3map))
		G_fatal_error(_("Unable to read range of 3D raster map <%s>"), name);

	    G3d_range_min_max(g3map, &dmin, &dmax);
	    fprintf(out, "min=%f\n", dmin);
	    fprintf(out, "max=%f\n", dmax);

	}			/*Region */
	else if (gflag->answer) {
	    sprintf(tmp1, "%f", cellhd.north);
	    sprintf(tmp2, "%f", cellhd.south);
	    G_trim_decimal(tmp1);
	    G_trim_decimal(tmp2);
	    fprintf(out, "north=%s\n", tmp1);
	    fprintf(out, "south=%s\n", tmp2);

	    sprintf(tmp1, "%f", cellhd.east);
	    sprintf(tmp2, "%f", cellhd.west);
	    G_trim_decimal(tmp1);
	    G_trim_decimal(tmp2);
	    fprintf(out, "east=%s\n", tmp1);
	    fprintf(out, "west=%s\n", tmp2);

	    fprintf(out, "bottom=%g\n", cellhd.bottom);
	    fprintf(out, "top=%g\n", cellhd.top);
	}			/*Resolution */
	else if (sflag->answer) {
	    G_format_resolution(cellhd.ns_res, tmp3, cellhd.proj);
	    fprintf(out, "nsres=%s\n", tmp3);

	    G_format_resolution(cellhd.ew_res, tmp3, cellhd.proj);
	    fprintf(out, "ewres=%s\n", tmp3);

	    fprintf(out, "tbres=%g\n", cellhd.tb_res);
	}			/*Datatype */
	else if (tflag->answer) {
	    fprintf(out, "datatype=\"%s\"\n",
		    (data_type ==
		     FCELL_TYPE ? "float" : (data_type ==
					     DCELL_TYPE ? "double" : "??")));

	}			/*History output */
	else if (hflag->answer) {
	    if (hist_ok) {
		fprintf(out, "Data Source:\n");
		fprintf(out, "   %s\n", hist.datsrc_1);
		fprintf(out, "   %s\n", hist.datsrc_2);
		fprintf(out, "Data Description:\n");
		fprintf(out, "   %s\n", hist.keywrd);
		if (hist.edlinecnt) {
		    fprintf(out, "Comments:\n");
		    for (i = 0; i < hist.edlinecnt; i++)
			fprintf(out, "   %s\n", hist.edhist[i]);
		}
	    }
	    else {
		G_fatal_error(_("Error while reading history file"));
	    }
	}			/*Timestamp */
	else if (timestampflag->answer) {
	    if (time_ok && (first_time_ok || second_time_ok)) {

		G_format_timestamp(&ts, timebuff);

		/*Create the r.info timestamp string */
		fprintf(out, "Timestamp=\"%s\"", timebuff);

	    }
	    else {
		fprintf(out, "Timestamp=\"none\"\n");
	    }
	}

    }

    /*Close the opened map */
    if (!G3d_closeCell(g3map))
	G_fatal_error(_("Unable to close 3D raster map <%s>"), name);


    return 0;
}

/**************************************************************************/
int format_double(double value, char *buf)
{

    sprintf(buf, "%.8lf", value);
    G_trim_decimal(buf);
    return 0;
}
