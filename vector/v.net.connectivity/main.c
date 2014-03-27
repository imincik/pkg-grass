
/****************************************************************
 *
 * MODULE:     v.net.connectivity
 *
 * AUTHOR(S):  Daniel Bundala
 *
 * PURPOSE:    Vertex connectivity between two sets of nodes
 *
 * COPYRIGHT:  (C) 2002-2005 by the GRASS Development Team
 *
 *             This program is free software under the
 *             GNU General Public License (>=v2).
 *             Read the file COPYING that comes with GRASS
 *             for details.
 *
 ****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/glocale.h>
#include <grass/dbmi.h>
#include <grass/neta.h>


int main(int argc, char *argv[])
{
    struct Map_info In, Out;
    static struct line_pnts *Points;
    struct line_cats *Cats;
    struct GModule *module;	/* GRASS module for parsing arguments */
    struct Option *map_in, *map_out;
    struct Option *field_opt, *ncol;
    struct Option *catset1_opt, *fieldset1_opt, *whereset1_opt;
    struct Option *catset2_opt, *fieldset2_opt, *whereset2_opt;
    int with_z;
    int layer, mask_type;
    struct varray *varray_set1, *varray_set2;
    dglGraph_s *graph;
    int i, nnodes, nlines, *flow, total_flow, nedges;
    struct ilist *set1_list, *set2_list, *cut;
    int *node_costs;

    dglGraph_s vg;

    /* initialize GIS environment */
    G_gisinit(argv[0]);		/* reads grass env, stores program name to G_program_name() */

    /* initialize module */
    module = G_define_module();
    module->keywords = _("vector, network, connectivity");
    module->description =
	_("Computes vertex connectivity between two sets of nodes in the network.");

    /* Define the different options as defined in gis.h */
    map_in = G_define_standard_option(G_OPT_V_INPUT);
    field_opt = G_define_standard_option(G_OPT_V_FIELD);

    map_out = G_define_standard_option(G_OPT_V_OUTPUT);

    ncol = G_define_standard_option(G_OPT_COLUMN);
    ncol->key = "ncolumn";
    ncol->required = NO;
    ncol->description = _("Name of node capacity column");

    fieldset1_opt = G_define_standard_option(G_OPT_V_FIELD);
    fieldset1_opt->key = "set1_layer";
    fieldset1_opt->label = _("Set1 layer number or name");
    fieldset1_opt->guisection = _("Set1");

    catset1_opt = G_define_standard_option(G_OPT_V_CATS);
    catset1_opt->key = "set1_cats";
    catset1_opt->label = _("Set1 category values");
    catset1_opt->guisection = _("Set1");

    whereset1_opt = G_define_standard_option(G_OPT_WHERE);
    whereset1_opt->key = "set1_where";
    whereset1_opt->label =
	_("Set1 WHERE conditions of SQL statement without 'where' keyword");
    whereset1_opt->guisection = _("Set1");

    fieldset2_opt = G_define_standard_option(G_OPT_V_FIELD);
    fieldset2_opt->key = "set2_layer";
    fieldset2_opt->description = _("Set2 layer number or name");
    fieldset2_opt->guisection = _("Set2");

    catset2_opt = G_define_standard_option(G_OPT_V_CATS);
    catset2_opt->key = "set2_cats";
    catset2_opt->description = _("Set2 category values");
    catset2_opt->guisection = _("Set2");

    whereset2_opt = G_define_standard_option(G_OPT_WHERE);
    whereset2_opt->key = "set2_where";
    whereset2_opt->label =
	_("Set2 WHERE conditions of SQL statement without 'where' keyword");
    whereset2_opt->guisection = _("Set2");

    /* options and flags parser */
    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);
    /* TODO: make an option for this */
    mask_type = GV_LINE | GV_BOUNDARY;

    Points = Vect_new_line_struct();
    Cats = Vect_new_cats_struct();

    Vect_check_input_output_name(map_in->answer, map_out->answer,
				 GV_FATAL_EXIT);

    Vect_set_open_level(2);

    if (1 > Vect_open_old(&In, map_in->answer, ""))
	G_fatal_error(_("Unable to open vector map <%s>"), map_in->answer);

    with_z = Vect_is_3d(&In);

    if (0 > Vect_open_new(&Out, map_out->answer, with_z)) {
	Vect_close(&In);
	G_fatal_error(_("Unable to create vector map <%s>"), map_out->answer);
    }

    /* parse filter option and select appropriate lines */
    layer = atoi(field_opt->answer);

    if (NetA_initialise_varray
	(&In, atoi(fieldset1_opt->answer), GV_POINT, whereset1_opt->answer,
	 catset1_opt->answer, &varray_set1) == 2)
	G_fatal_error(_("Neither %s nor %s was given"), catset1_opt->key,
		      whereset1_opt->key);
    if (NetA_initialise_varray
	(&In, atoi(fieldset2_opt->answer), GV_POINT, whereset2_opt->answer,
	 catset2_opt->answer, &varray_set2) == 2)
	G_fatal_error(_("Neither %s nor %s was given"), catset2_opt->key,
		      whereset2_opt->key);

    set1_list = Vect_new_list();
    set2_list = Vect_new_list();

    NetA_varray_to_nodes(&In, varray_set1, set1_list, NULL);
    NetA_varray_to_nodes(&In, varray_set2, set2_list, NULL);

    nlines = Vect_get_num_lines(&In);
    nnodes = Vect_get_num_nodes(&In);

    if (set1_list->n_values == 0)
	G_fatal_error(_("%s is empty"), "set1");

    if (set2_list->n_values == 0)
	G_fatal_error(_("%s is empty"), "set2");

    Vect_copy_head_data(&In, &Out);
    Vect_hist_copy(&In, &Out);
    Vect_hist_command(&Out);

    Vect_net_build_graph(&In, mask_type, 0, atoi(field_opt->answer),
			 NULL, NULL, NULL, 0, 0);
    graph = &(In.graph);

    /*build new graph */
    if (ncol->answer) {
	node_costs = (int *)G_calloc(nnodes + 1, sizeof(int));
	if (!node_costs)
	    G_fatal_error(_("Out of memory"));
	NetA_get_node_costs(&In, layer, ncol->answer, node_costs);
	nedges = NetA_split_vertices(graph, &vg, node_costs);
	G_free(node_costs);
    }
    else
	nedges = NetA_split_vertices(graph, &vg, NULL);
    graph = &vg;

    for (i = 0; i < set1_list->n_values; i++)
	set1_list->value[i] = set1_list->value[i] * 2;	/*out vertex */
    for (i = 0; i < set2_list->n_values; i++)
	set2_list->value[i] = set2_list->value[i] * 2 - 1;	/*in vertex */

    flow = (int *)G_calloc(nedges + 1, sizeof(int));
    if (!flow)
	G_fatal_error(_("Out of memory"));

    total_flow = NetA_flow(graph, set1_list, set2_list, flow);
    G_debug(3, "Connectivity: %d", total_flow);
    cut = Vect_new_list();
    total_flow = NetA_min_cut(graph, set1_list, set2_list, flow, cut);

    /*TODO: copy old points */
    for (i = 0; i < cut->n_values; i++)
	NetA_add_point_on_node(&In, &Out, cut->value[i], Cats);

    Vect_destroy_list(cut);

    G_free(flow);
    Vect_destroy_list(set1_list);
    Vect_destroy_list(set2_list);

    Vect_build(&Out);

    Vect_close(&In);
    Vect_close(&Out);

    exit(EXIT_SUCCESS);
}
