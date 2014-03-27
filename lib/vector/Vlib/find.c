/*!
 * \file find.c
 *
 * \brief Vector library - Find nearest vector feature.
 *
 * Higher level functions for reading/writing/manipulating vectors.
 *
 * \author Original author CERL, probably Dave Gerdes or Mike
 * Higgins. Update to GRASS 5.7 Radim Blazek and David D. Gray.
 *
 * (C) 2001-2007 by the GRASS Development Team
 *
 * This program is free software under the GNU General Public
 *              License (>=v2). Read the file COPYING that comes with GRASS
 *              for details.
 */

#include <math.h>
#include <grass/gis.h>
#include <grass/Vect.h>

#ifndef HUGE_VAL
#define HUGE_VAL 9999999999999.0
#endif

/*!
 * \brief Find the nearest node.
 *
 * \param[in] Map vector map
 * \param[in] ux,uy,uz point coordinates 
 * \param[in] maxdist max distance from the line
 * \param[in] with_z 3D (WITH_Z, WITHOUT_Z)
 *
 * \return number of nearest node
 * \return 0 if not found
 */
int
Vect_find_node(struct Map_info *Map,
	       double ux, double uy, double uz, double maxdist, int with_z)
{
    int i, nnodes, node;
    BOUND_BOX box;
    struct ilist *NList;
    double x, y, z;
    double cur_dist, dist;

    G_debug(3, "Vect_find_node() for %f %f %f maxdist = %f", ux, uy, uz,
	    maxdist);
    NList = Vect_new_list();

    /* Select all nodes in box */
    box.N = uy + maxdist;
    box.S = uy - maxdist;
    box.E = ux + maxdist;
    box.W = ux - maxdist;
    if (with_z) {
	box.T = uz + maxdist;
	box.B = uz - maxdist;
    }
    else {
	box.T = HUGE_VAL;
	box.B = -HUGE_VAL;
    }

    nnodes = Vect_select_nodes_by_box(Map, &box, NList);
    G_debug(3, " %d nodes in box", nnodes);

    if (nnodes == 0)
	return 0;

    /* find nearest */
    cur_dist = PORT_DOUBLE_MAX;
    node = 0;
    for (i = 0; i < nnodes; i++) {
	Vect_get_node_coor(Map, NList->value[i], &x, &y, &z);
	dist = Vect_points_distance(ux, uy, uz, x, y, z, with_z);
	if (dist < cur_dist) {
	    cur_dist = dist;
	    node = i;
	}
    }
    G_debug(3, "  nearest node %d in distance %f", NList->value[node],
	    cur_dist);

    /* Check if in max distance */
    if (cur_dist <= maxdist)
	return (NList->value[node]);
    else
	return 0;
}

/*!
 * \brief Find the nearest line.
 *
 * \param[in] map vector map
 * \param[in] ux,uy,uz points coordinates
 * \param[in] type feature type (GV_LINE, GV_POINT, GV_BOUNDARY or GV_CENTROID)
 * if only want to search certain types of lines or -1 if search all lines
 * \param[in] maxdist max distance from the line
 * \param[in] with_z 3D (WITH_Z, WITHOUT_Z)
 * \param[in] exclude if > 0 number of line which should be excluded from selection.
 * May be useful if we need line nearest to other one. 
 *
 * \return number of nearest line
 * \return 0 if not found
 *
 */
/* original dig_point_to_line() in grass50 */
int
Vect_find_line(struct Map_info *map,
	       double ux, double uy, double uz,
	       int type, double maxdist, int with_z, int exclude)
{
    int line;
    struct ilist *exclude_list;

    exclude_list = Vect_new_list();

    Vect_list_append(exclude_list, exclude);

    line = Vect_find_line_list(map, ux, uy, uz,
			       type, maxdist, with_z, exclude_list, NULL);

    Vect_destroy_list(exclude_list);

    return line;
}

/*!
 * \brief Find the nearest line(s).
 *
 * \param[in] map vector map
 * \param[in] ux,uy,uz points coordinates
 * \param[in] type feature type (GV_LINE, GV_POINT, GV_BOUNDARY or GV_CENTROID)
 * if only want to search certain types of lines or -1 if search all lines
 * \param[in] maxdist max distance from the line
 * \param[in] with_z 3D (WITH_Z, WITHOUT_Z)
 * \param[in] exclude list of lines which should be excluded from selection
 * \param[in] found list of found lines (or NULL)
 *
 * \return number of nearest line
 * \return 0 if not found
 */
int
Vect_find_line_list(struct Map_info *map,
		    double ux, double uy, double uz,
		    int type, double maxdist, int with_z,
		    struct ilist *exclude, struct ilist *found)
{
    int choice;
    double new_dist;
    double cur_dist;
    int gotone;
    int i, line;
    static struct line_pnts *Points;
    static int first_time = 1;
    struct Plus_head *Plus;
    BOUND_BOX box;
    struct ilist *List;

    G_debug(3, "Vect_find_line_list() for %f %f %f type = %d maxdist = %f",
	    ux, uy, uz, type, maxdist);

    if (first_time) {
	Points = Vect_new_line_struct();
	first_time = 0;
    }

    Plus = &(map->plus);
    gotone = 0;
    choice = 0;
    cur_dist = HUGE_VAL;

    box.N = uy + maxdist;
    box.S = uy - maxdist;
    box.E = ux + maxdist;
    box.W = ux - maxdist;
    if (with_z) {
	box.T = uz + maxdist;
	box.B = uz - maxdist;
    }
    else {
	box.T = PORT_DOUBLE_MAX;
	box.B = -PORT_DOUBLE_MAX;
    }

    List = Vect_new_list();

    if (found)
	Vect_reset_list(found);

    Vect_select_lines_by_box(map, &box, type, List);
    for (i = 0; i < List->n_values; i++) {
	line = List->value[i];
	if (Vect_val_in_list(exclude, line)) {
	    G_debug(3, " line = %d exclude", line);
	    continue;
	}

	/* No more needed */
	/*
	   Line = Plus->Line[line];       
	   if ( Line == NULL ) continue;
	   if ( !(type & Line->type) ) continue; 
	 */

	Vect_read_line(map, Points, NULL, line);

	Vect_line_distance(Points, ux, uy, uz, with_z, NULL, NULL, NULL,
			   &new_dist, NULL, NULL);
	G_debug(3, " line = %d distance = %f", line, new_dist);

	if (found && new_dist <= maxdist) {
	    Vect_list_append(found, line);
	}

	if ((++gotone == 1) || (new_dist <= cur_dist)) {
	    if (new_dist == cur_dist) {
		/* TODO */
		/* choice = dig_center_check (map->Line, choice, a, ux, uy); */
		continue;
	    }

	    choice = line;
	    cur_dist = new_dist;
	}
    }

    G_debug(3, "min distance found = %f", cur_dist);
    if (cur_dist > maxdist)
	choice = 0;

    Vect_destroy_list(List);

    return (choice);
}

/*!
 * \brief Find the nearest area
 *
 * \param[in] Map vector map
 * \param[in] x,y point coordinates
 *
 * \return area number
 * \return 0 if not found
 */
/* original dig_point_to_area() in grass50 */
int Vect_find_area(struct Map_info *Map, double x, double y)
{
    int i, ret, area;
    static int first = 1;
    BOUND_BOX box;
    static struct ilist *List;

    G_debug(3, "Vect_find_area() x = %f y = %f", x, y);

    if (first) {
	List = Vect_new_list();
	first = 0;
    }

    /* select areas by box */
    box.E = x;
    box.W = x;
    box.N = y;
    box.S = y;
    box.T = PORT_DOUBLE_MAX;
    box.B = -PORT_DOUBLE_MAX;
    Vect_select_areas_by_box(Map, &box, List);
    G_debug(3, "  %d areas selected by box", List->n_values);

    for (i = 0; i < List->n_values; i++) {
	area = List->value[i];
	ret = Vect_point_in_area(Map, area, x, y);

	G_debug(3, "    area = %d Vect_point_in_area() = %d", area, ret);

	if (ret >= 1)
	    return (area);
    }

    return 0;
}

/*!
 * \brief Find the nearest island
 * 
 * \param[in] Map vector map
 * \param[in] x,y points coordinates
 *
 * \return island number,
 * \return 0 if not found
 */
/* original dig_point_to_area() in grass50 */
int Vect_find_island(struct Map_info *Map, double x, double y)
{
    int i, ret, island, current, current_size, size;
    static int first = 1;
    BOUND_BOX box;
    static struct ilist *List;
    static struct line_pnts *Points;

    G_debug(3, "Vect_find_island() x = %f y = %f", x, y);

    if (first) {
	List = Vect_new_list();
	Points = Vect_new_line_struct();
	first = 0;
    }

    /* select islands by box */
    box.E = x;
    box.W = x;
    box.N = y;
    box.S = y;
    box.T = PORT_DOUBLE_MAX;
    box.B = -PORT_DOUBLE_MAX;
    Vect_select_isles_by_box(Map, &box, List);
    G_debug(3, "  %d islands selected by box", List->n_values);

    current_size = -1;
    current = 0;
    for (i = 0; i < List->n_values; i++) {
	island = List->value[i];
	ret = Vect_point_in_island(x, y, Map, island);

	if (ret >= 1) {		/* inside */
	    if (current > 0) {	/* not first */
		if (current_size == -1) {	/* second */
		    G_begin_polygon_area_calculations();
		    Vect_get_isle_points(Map, current, Points);
		    current_size =
			G_area_of_polygon(Points->x, Points->y,
					  Points->n_points);
		}

		Vect_get_isle_points(Map, island, Points);
		size =
		    G_area_of_polygon(Points->x, Points->y, Points->n_points);

		if (size < current_size) {
		    current = island;
		    current_size = size;
		}
	    }
	    else {		/* first */
		current = island;
	    }
	}
    }

    return current;
}
