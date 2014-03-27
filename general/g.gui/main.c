
/****************************************************************************
 *
 * MODULE:       g.gui
 *
 * AUTHOR(S):    Martin Landa <landa.martin gmail.com>
 *		 Hamish Bowman <hamish_b yahoo com> (fine tuning)
 *
 * PURPOSE:      Start GRASS GUI from command line.
 *
 * COPYRIGHT:    (C) 2008 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include <grass/spawn.h>

int main(int argc, char *argv[])
{
    struct Option *type, *rc_file;
    struct Flag *update, *nolaunch;
    struct GModule *module;
    char *gui_type_env;
    char progname[GPATH_MAX];

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("general, gui");
    module->description =
	_("Launches a GRASS graphical user interface (GUI) session.");

    type = G_define_option();
    type->key = "gui";
    type->type = TYPE_STRING;
    type->label = _("GUI type");
    type->description = _("Default value: GRASS_GUI if defined, otherwise wxpython");
    type->descriptions = _("wxpython;wxPython based GUI - wxGUI;"
			   "tcltk;Tcl/Tk based GUI - GIS Manager (gis.m);"
			   "oldtcltk;Old Tcl/Tk based GUI - Display Manager (d.m);"
			   "text;command line interface only");
    type->options = "wxpython,tcltk,oldtcltk,text";

    rc_file = G_define_standard_option(G_OPT_F_INPUT);
    rc_file->key = "workspace";
    rc_file->required = NO;
    rc_file->description = _("Name of workspace file");

    update = G_define_flag();
    update->key = 'u';
    update->description = _("Update default GUI setting");

    nolaunch = G_define_flag();
    nolaunch->key = 'n';
    nolaunch->description =
	_("Do not launch GUI after updating the default GUI setting");

    if (argc > 1 && G_parser(argc, argv))
	exit(EXIT_FAILURE);


    if( type->answer && strcmp(type->answer, "text") == 0
					&& !nolaunch->answer)
	nolaunch->answer = TRUE;

    if(nolaunch->answer && !update->answer)
	update->answer = TRUE;

    gui_type_env = G__getenv("GRASS_GUI");

    if (!type->answer) {
	if (gui_type_env && strcmp(gui_type_env, "text")) {
	    type->answer = G_store(gui_type_env);
	}
	else {
	    type->answer = "wxpython";
	}
    }

    if (((gui_type_env && update->answer) &&
	 strcmp(gui_type_env, type->answer) != 0) || !gui_type_env) {
	G_setenv("GRASS_GUI", type->answer);
	G_message(_("<%s> is now the default GUI"), type->answer);
    }
    else {
	if(update->answer)
	    if(gui_type_env) {
		G_debug(1, "No change: old gui_type_env=[%s], new type->ans=[%s]",
			gui_type_env, type->answer);
	    }
    }

    if(nolaunch->answer)
	exit(EXIT_SUCCESS);

    G_message(_("Launching '%s' GUI in the background, please wait ..."), type->answer);

    if (strcmp(type->answer, "oldtcltk") == 0) {
#ifdef __MINGW32__
	G_fatal_error(_("The old d.m GUI is not available for WinGRASS"));
#endif
	sprintf(progname, "%s/etc/dm/d.m.tcl", G_gisbase());
	if (rc_file->answer) {
	    G_spawn_ex(getenv("GRASS_WISH"), "d.m", progname, "-name", "d_m_tcl",
		    rc_file->answer, SF_BACKGROUND, NULL);
	}
	else {
	    G_spawn_ex(getenv("GRASS_WISH"), "d.m", progname, "-name", "d_m_tcl",
		    SF_BACKGROUND, NULL);
	}
    }
    else if (strcmp(type->answer, "tcltk") == 0) {
	sprintf(progname, "%s/etc/gm/gm.tcl", G_gisbase());
	if (rc_file->answer) {
	    G_spawn_ex(getenv("GRASS_WISH"), "gis.m", progname, "-name",
		    "gm_tcl", rc_file->answer, SF_BACKGROUND, NULL);
	}
	else {
	    G_spawn_ex(getenv("GRASS_WISH"), "gis.m", progname, "-name",
		    "gm_tcl", SF_BACKGROUND, NULL);
	}
    }
    else if (strcmp(type->answer, "wxpython") == 0) {
	sprintf(progname, "%s/etc/wxpython/wxgui.py", G_gisbase());
	if (rc_file->answer) {
	    G_spawn_ex(getenv("GRASS_PYTHON"), "wxgui", progname,
		    "--workspace", rc_file->answer, SF_BACKGROUND, NULL);
	}
	else {
	    G_spawn_ex(getenv("GRASS_PYTHON"), "wxgui", progname,
		    SF_BACKGROUND, NULL);
	}
    }

    /* stop the impatient from starting it again before the
       splash screen comes up */
    G_sleep(3);

    exit(EXIT_SUCCESS);
}
