
/****************************************************************
 * 
 *  MODULE:       v.net.alloc
 *  
 *  AUTHOR(S):    Radim Blazek
 *                
 *  PURPOSE:      Allocate subnets for nearest centres.
 *                
 *  COPYRIGHT:    (C) 2001 by the GRASS Development Team
 * 
 *                This program is free software under the 
 *                GNU General Public License (>=v2). 
 *                Read the file COPYING that comes with GRASS
 *                for details.
 * 
 **************************************************************/
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>

typedef struct
{
    int cat;			/* category number */
    int node;			/* node number */
} CENTER;

typedef struct
{
    int centre;			/* neares centre, initially -1 */
    double cost;		/* costs from this centre, initially not undefined */
} NODE;

int main(int argc, char **argv)
{
    int i, j, ret, centre, line, centre1, centre2;
    int nlines, nnodes, type, ltype, afield, nfield, geo, cat;
    int node, node1, node2;
    double cost, e1cost, e2cost, n1cost, n2cost, s1cost, s2cost, l, l1, l2;
    struct Option *map, *output;
    struct Option *afield_opt, *nfield_opt, *afcol, *abcol, *ncol, *type_opt,
	*term_opt;
    struct Flag *geo_f;
    struct GModule *module;
    char *mapset;
    struct Map_info Map, Out;
    struct cat_list *catlist;
    CENTER *Centers = NULL;
    int acentres = 0, ncentres = 0;
    NODE *Nodes;
    struct line_cats *Cats;
    struct line_pnts *Points, *SPoints;

    G_gisinit(argv[0]);

    module = G_define_module();
    module->keywords = _("vector, network, allocation");
    module->label =
	_("Allocate subnets for nearest centres (direction from centre).");
    module->description =
	_("Centre node must be opened (costs >= 0). "
	  "Costs of centre node are used in calculation");


    map = G_define_standard_option(G_OPT_V_INPUT);
    output = G_define_standard_option(G_OPT_V_OUTPUT);

    type_opt = G_define_standard_option(G_OPT_V_TYPE);
    type_opt->options = "line,boundary";
    type_opt->answer = "line,boundary";
    type_opt->description = _("Arc type");

    afield_opt = G_define_standard_option(G_OPT_V_FIELD);
    afield_opt->key = "alayer";
    afield_opt->answer = "1";
    afield_opt->description = _("Arc layer");

    nfield_opt = G_define_standard_option(G_OPT_V_FIELD);
    nfield_opt->key = "nlayer";
    nfield_opt->answer = "2";
    nfield_opt->description = _("Node layer");

    afcol = G_define_option();
    afcol->key = "afcolumn";
    afcol->type = TYPE_STRING;
    afcol->required = NO;
    afcol->description =
	_("Arc forward/both direction(s) cost column (number)");

    abcol = G_define_option();
    abcol->key = "abcolumn";
    abcol->type = TYPE_STRING;
    abcol->required = NO;
    abcol->description = _("Arc backward direction cost column (number)");

    ncol = G_define_option();
    ncol->key = "ncolumn";
    ncol->type = TYPE_STRING;
    ncol->required = NO;
    ncol->description = _("Node cost column (number)");

    term_opt = G_define_standard_option(G_OPT_V_CATS);
    term_opt->key = "ccats";
    term_opt->required = YES;
    term_opt->description =
	_("Categories of centres (points on nodes) to which net "
	  "will be allocated, "
	  "layer for this categories is given by nlayer option");

    geo_f = G_define_flag();
    geo_f->key = 'g';
    geo_f->description =
	_("Use geodesic calculation for longitude-latitude locations");

    if (G_parser(argc, argv))
	exit(EXIT_FAILURE);

    Vect_check_input_output_name(map->answer, output->answer, GV_FATAL_EXIT);

    Cats = Vect_new_cats_struct();
    Points = Vect_new_line_struct();
    SPoints = Vect_new_line_struct();

    type = Vect_option_to_types(type_opt);
    afield = atoi(afield_opt->answer);
    nfield = atoi(nfield_opt->answer);

    catlist = Vect_new_cat_list();
    Vect_str_to_cat_list(term_opt->answer, catlist);

    if (geo_f->answer)
	geo = 1;
    else
	geo = 0;

    mapset = G_find_vector2(map->answer, NULL);

    if (mapset == NULL)
	G_fatal_error(_("Vector map <%s> not found"), map->answer);

    Vect_set_open_level(2);
    Vect_open_old(&Map, map->answer, mapset);

    /* Build graph */
    Vect_net_build_graph(&Map, type, afield, nfield, afcol->answer,
			 abcol->answer, ncol->answer, geo, 0);

    nnodes = Vect_get_num_nodes(&Map);

    /* Create list of centres based on list of categories */
    for (node = 1; node <= nnodes; node++) {
	nlines = Vect_get_node_n_lines(&Map, node);
	for (j = 0; j < nlines; j++) {
	    line = abs(Vect_get_node_line(&Map, node, j));
	    ltype = Vect_read_line(&Map, NULL, Cats, line);
	    if (!(ltype & GV_POINT))
		continue;
	    if (!(Vect_cat_get(Cats, nfield, &cat)))
		continue;
	    if (Vect_cat_in_cat_list(cat, catlist)) {
		Vect_net_get_node_cost(&Map, node, &n1cost);
		if (n1cost == -1) {	/* closed */
		    G_warning("Centre at closed node (costs = -1) ignored");
		}
		else {
		    if (acentres == ncentres) {
			acentres += 1;
			Centers =
			    (CENTER *) G_realloc(Centers,
						 acentres * sizeof(CENTER));
		    }
		    Centers[ncentres].cat = cat;
		    Centers[ncentres].node = node;
		    G_debug(2, "centre = %d node = %d cat = %d", ncentres,
			    node, cat);
		    ncentres++;
		}
	    }
	}
    }

    G_message(_("Number of centres: [%d] (nlayer: [%d])"), ncentres, nfield);

    if (ncentres == 0)
	G_warning(_("Not enough centres for selected nlayer. "
		    "Nothing will be allocated."));

    /* alloc and reset space for all nodes */
    Nodes = (NODE *) G_calloc((nnodes + 1), sizeof(NODE));
    for (i = 1; i <= nnodes; i++) {
	Nodes[i].centre = -1;
    }


    /* Fill Nodes by neares centre and costs from that centre */
    G_message(_("Calculating costs from centres ..."));

    for (centre = 0; centre < ncentres; centre++) {
	G_percent(centre, ncentres, 1);
	node1 = Centers[centre].node;
	Vect_net_get_node_cost(&Map, node1, &n1cost);
	G_debug(2, "centre = %d node = %d cat = %d", centre, node1,
		Centers[centre].cat);
	for (node2 = 1; node2 <= nnodes; node2++) {
	    G_debug(5, "  node1 = %d node2 = %d", node1, node2);
	    Vect_net_get_node_cost(&Map, node2, &n2cost);
	    if (n2cost == -1) {
		continue;
	    }			/* closed, left it as not attached */

	    ret = Vect_net_shortest_path(&Map, node1, node2, NULL, &cost);
	    if (ret == -1) {
		continue;
	    }			/* node unreachable */

	    /* We must add centre node costs (not calculated by Vect_net_shortest_path() ), but
	     *  only if centre and node are not identical, because at the end node cost is add later */
	    if (node1 != node2)
		cost += n1cost;

	    G_debug(5,
		    "Arc nodes: %d %d cost: %f (x old cent: %d old cost %f",
		    node1, node2, cost, Nodes[node2].centre,
		    Nodes[node2].cost);
	    if (Nodes[node2].centre == -1 || cost < Nodes[node2].cost) {
		Nodes[node2].cost = cost;
		Nodes[node2].centre = centre;
	    }
	}
    }
    G_percent(1, 1, 1);

    /* Write arcs to new map */
    Vect_open_new(&Out, output->answer, Vect_is_3d(&Map));
    Vect_hist_command(&Out);

    nlines = Vect_get_num_lines(&Map);
    for (line = 1; line <= nlines; line++) {
	ltype = Vect_read_line(&Map, Points, NULL, line);
	if (!(ltype & type)) {
	    continue;
	}
	Vect_get_line_nodes(&Map, line, &node1, &node2);
	centre1 = Nodes[node1].centre;
	centre2 = Nodes[node2].centre;
	s1cost = Nodes[node1].cost;
	s2cost = Nodes[node2].cost;
	G_debug(3, "Line %d:", line);
	G_debug(3, "Arc centres: %d %d (nodes: %d %d)", centre1, centre2,
		node1, node2);

	Vect_net_get_node_cost(&Map, node1, &n1cost);
	Vect_net_get_node_cost(&Map, node2, &n2cost);

	Vect_net_get_line_cost(&Map, line, GV_FORWARD, &e1cost);
	Vect_net_get_line_cost(&Map, line, GV_BACKWARD, &e2cost);

	G_debug(3, "  s1cost = %f n1cost = %f e1cost = %f", s1cost, n1cost,
		e1cost);
	G_debug(3, "  s2cost = %f n2cost = %f e2cost = %f", s2cost, n2cost,
		e2cost);

	Vect_reset_cats(Cats);

	/* First check if arc is reachable from at least one side */
	if ((centre1 != -1 && n1cost != -1 && e1cost != -1) ||
	    (centre2 != -1 && n2cost != -1 && e2cost != -1)) {
	    /* Line is reachable at least from one side */
	    G_debug(3, "  -> arc is reachable");

	    if (centre1 == centre2) {	/* both nodes in one area -> whole arc in one area */
		if (centre1 != -1)
		    cat = Centers[centre1].cat;	/* line reachable */
		else
		    cat = Centers[centre2].cat;
		Vect_cat_set(Cats, 1, cat);
		Vect_write_line(&Out, ltype, Points, Cats);
	    }
	    else {		/* each node in different area */
		/* Check if direction is reachable */
		if (centre1 == -1 || n1cost == -1 || e1cost == -1) {	/* closed from first node */
		    G_debug(3,
			    "    -> arc is not reachable from 1. node -> alloc to 2. node");
		    cat = Centers[centre2].cat;
		    Vect_cat_set(Cats, 1, cat);
		    Vect_write_line(&Out, ltype, Points, Cats);
		    continue;
		}
		else if (centre2 == -1 || n2cost == -1 || e2cost == -1) {	/* closed from second node */
		    G_debug(3,
			    "    -> arc is not reachable from 2. node -> alloc to 1. node");
		    cat = Centers[centre1].cat;
		    Vect_cat_set(Cats, 1, cat);
		    Vect_write_line(&Out, ltype, Points, Cats);
		    continue;
		}

		/* Now we know that arc is reachable from both sides */

		/* Add costs of node to starting costs */
		s1cost += n1cost;
		s2cost += n2cost;

		/* Check if s1cost + e1cost <= s2cost or s2cost + e2cost <= s1cost !
		 * Note this check also possibility of (e1cost + e2cost) = 0 */
		if (s1cost + e1cost <= s2cost) {	/* whole arc reachable from node1 */
		    cat = Centers[centre1].cat;
		    Vect_cat_set(Cats, 1, cat);
		    Vect_write_line(&Out, ltype, Points, Cats);
		}
		else if (s2cost + e2cost <= s1cost) {	/* whole arc reachable from node2 */
		    cat = Centers[centre2].cat;
		    Vect_cat_set(Cats, 1, cat);
		    Vect_write_line(&Out, ltype, Points, Cats);
		}
		else {		/* split */
		    /* Calculate relative costs - we expect that costs along the line do not change */
		    l = Vect_line_length(Points);
		    e1cost /= l;
		    e2cost /= l;

		    G_debug(3, "  -> s1cost = %f e1cost = %f", s1cost,
			    e1cost);
		    G_debug(3, "  -> s2cost = %f e2cost = %f", s2cost,
			    e2cost);

		    /* Costs from both centres to the splitting point must be equal:
		     * s1cost + l1 * e1cost = s2cost + l2 * e2cost */
		    l1 = (l * e2cost - s1cost + s2cost) / (e1cost + e2cost);
		    l2 = l - l1;
		    G_debug(3, "l = %f l1 = %f l2 = %f", l, l1, l2);

		    /* First segment */
		    ret = Vect_line_segment(Points, 0, l1, SPoints);
		    if (ret == 0) {
			G_warning(_("Cannot get line segment, segment out of line"));
		    }
		    else {
			cat = Centers[centre1].cat;
			Vect_cat_set(Cats, 1, cat);
			Vect_write_line(&Out, ltype, SPoints, Cats);
		    }

		    /* Second segment */
		    ret = Vect_line_segment(Points, l1, l, SPoints);
		    if (ret == 0) {
			G_warning(_("Cannot get line segment, segment out of line"));
		    }
		    else {
			Vect_reset_cats(Cats);
			cat = Centers[centre2].cat;
			Vect_cat_set(Cats, 1, cat);
			Vect_write_line(&Out, ltype, SPoints, Cats);
		    }
		}
	    }
	}
	else {
	    /* arc is not reachable */
	    G_debug(3, "  -> arc is not reachable");
	    Vect_write_line(&Out, ltype, Points, Cats);
	}
    }

    Vect_build(&Out);

    /* Free, ... */
    G_free(Nodes);
    G_free(Centers);
    Vect_close(&Map);
    Vect_close(&Out);

    exit(EXIT_SUCCESS);
}
