/*
 ****************************************************************************
 *
 * MODULE:     v.out.ascii
 * AUTHOR(S):  Michael Higgins, U.S. Army Construction Engineering Research Laboratory
 *             James Westervelt, U.S. Army Construction Engineering Research Laboratory
 *             Radim Blazek, ITC-Irst, Trento, Italy
 *             Martin Landa, CTU in Prague, Czech Republic (v.out.ascii.db merged)
 *
 * PURPOSE:    v.out.ascii: writes GRASS vector data as ASCII files
 * COPYRIGHT:  (C) 2000-2008 by the GRASS Development Team
 *
 *             This program is free software under the GNU General Public
 *              License (>=v2). Read the file COPYING that comes with GRASS
 *              for details.
 *
 ****************************************************************************
 */
/*  @(#)b_a_dig.c       2.1  6/26/87  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/glocale.h>
#include "local_proto.h"

int main(int argc, char *argv[])
{
    FILE *ascii, *att;
    struct Option *input, *output, *format_opt, *dp_opt, *delim_opt,
	*field_opt, *column_opt, *where_opt;
    struct Flag *verf, *region_flag;
    int format, dp, field;
    char *fs;
    struct Map_info Map;
    int ver = 5, pnt = 0;
    struct GModule *module;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("vector, export");
    module->description =
	_("Converts a GRASS binary vector map to a GRASS ASCII vector map.");

    input = G_define_standard_option(G_OPT_V_INPUT);

    output = G_define_standard_option(G_OPT_F_OUTPUT);
    output->required = NO;
    output->description =
	_("Path to resulting ASCII file or ASCII vector name if '-o' is defined");

    format_opt = G_define_option();
    format_opt->key = "format";
    format_opt->type = TYPE_STRING;
    format_opt->required = NO;
    format_opt->multiple = NO;
    format_opt->options = "point,standard";
    format_opt->answer = "point";
    format_opt->description = _("Output format");

    delim_opt = G_define_standard_option(G_OPT_F_SEP);
    delim_opt->description = _("Field separator (points mode)");
    delim_opt->guisection = _("Points");

    dp_opt = G_define_option();
    dp_opt->key = "dp";
    dp_opt->type = TYPE_INTEGER;
    dp_opt->required = NO;
    dp_opt->options = "0-32";
    dp_opt->answer = "8";	/* This value is taken from the lib settings in G_format_easting() */
    dp_opt->description =
	_("Number of significant digits (floating point only)");
    dp_opt->guisection = _("Points");

    field_opt = G_define_standard_option(G_OPT_V_FIELD);
    field_opt->guisection = _("Selection");

    column_opt = G_define_standard_option(G_OPT_COLUMNS);
    column_opt->description = _("Name of attribute column(s) to be exported (point mode)");
    column_opt->guisection = _("Points");
    
    where_opt = G_define_standard_option(G_OPT_WHERE);
    where_opt->guisection = _("Selection");

    verf = G_define_flag();
    verf->key = 'o';
    verf->description = _("Create old (version 4) ASCII file");

    region_flag = G_define_flag();
    region_flag->key = 'r';
    region_flag->description =
	_("Only export points falling within current 3D region (points mode)");
    region_flag->guisection = _("Points");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);


    field = atoi(field_opt->answer);
        
    if (format_opt->answer[0] == 'p')
	format = FORMAT_POINT;
    else
	format = FORMAT_ALL;

    if (format == FORMAT_ALL && column_opt->answer) {
	G_warning(_("Parameter '%s' ignored in standard mode"),
		  column_opt->key);
    }

    if (verf->answer)
	ver = 4;

    if (ver == 4 && format == FORMAT_POINT) {
	G_fatal_error(_("Format 'point' is not supported for old version"));
    }

    if (ver == 4 && output->answer == NULL) {
	G_fatal_error(_("'output' must be given for old version"));
    }

    /* the field separator */
    fs = delim_opt->answer;
    if (strcmp(fs, "\\t") == 0)
	fs = "\t";
    if (strcmp(fs, "tab") == 0)
	fs = "\t";
    if (strcmp(fs, "space") == 0)
	fs = " ";
    if (strcmp(fs, "comma") == 0)
	fs = ",";

    /*The precision of the output */
    if (dp_opt->answer) {
	if (sscanf(dp_opt->answer, "%d", &dp) != 1)
	    G_fatal_error(_("Failed to interpret 'dp' parameter as an integer"));
    }


    /* open with topology only if needed */
    if (format == FORMAT_ALL && where_opt->answer) {
	if (Vect_open_old(&Map, input->answer, "") < 2) /* topology required for areas */
	    G_warning(_("Unable to open vector map <%s> at topology level. "
			"Areas will not be processed."),
		      input->answer);
    }
    else {
	Vect_set_open_level(1); /* topology not needed */ 
	if (Vect_open_old(&Map, input->answer, "") < 0) 
	    G_fatal_error(_("Unable to open vector map <%s>"), input->answer); 
    }
    
    if (output->answer) {
	if (ver == 4) {
	    ascii = G_fopen_new("dig_ascii", output->answer);
	}
	else if (strcmp(output->answer, "-") == 0) {
	    ascii = stdout;
	}
	else {
	    ascii = fopen(output->answer, "w");
	}

	if (ascii == NULL) {
	    G_fatal_error(_("Unable to open file <%s>"), output->answer);
	}
    }
    else {
	ascii = stdout;
    }

    if (format == FORMAT_ALL) {
	write_head(ascii, &Map);
	fprintf(ascii, "VERTI:\n");
    }

    /* Open dig_att */
    att = NULL;
    if (ver == 4 && !pnt) {
	if (G_find_file("dig_att", output->answer, G_mapset()) != NULL)
	    G_fatal_error(_("dig_att file already exist"));

	if ((att = G_fopen_new("dig_att", output->answer)) == NULL)
	    G_fatal_error(_("Unable to open dig_att file <%s>"),
			  output->answer);
    }

    bin_to_asc(ascii, att, &Map, ver, format, dp, fs,
	       region_flag->answer, field, where_opt->answer,
	       column_opt->answers);
    
    if (ascii != NULL)
	fclose(ascii);
    if (att != NULL)
	fclose(att);

    Vect_close(&Map);

    exit(EXIT_SUCCESS);
}
