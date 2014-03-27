
/**
 * \file plus_line.c
 *
 * \brief Vector library - update topo for lines (lower level functions)
 *
 * Lower level functions for reading/writing/manipulating vectors.
 *
 * This program is free software under the GNU General Public License
 * (>=v2). Read the file COPYING that comes with GRASS for details.
 *
 * \author CERL (probably Dave Gerdes), Radim Blazek
 *
 * \date 2001-2008
 */

#include <stdlib.h>
#include <grass/Vect.h>

static int add_line(struct Plus_head *plus, int lineid, int type, struct line_pnts *Points,
		    long offset)
{
    int node, lp;
    P_LINE *line;
    BOUND_BOX box;

    plus->Line[lineid] = dig_alloc_line();
    line = plus->Line[lineid];

    /* Add nodes */
    G_debug(3, "Register node: type = %d,  %f,%f", type, Points->x[0],
	    Points->y[0]);

    node = dig_find_node(plus, Points->x[0], Points->y[0], Points->z[0]);
    G_debug(3, "node = %d", node);
    if (node == 0) {
	node = dig_add_node(plus, Points->x[0], Points->y[0], Points->z[0]);
	G_debug(3, "Add new node: %d", node);
    }
    else {
	G_debug(3, "Old node found: %d", node);
    }
    line->N1 = node;
    dig_node_add_line(plus, node, lineid, Points, type);
    if (plus->do_uplist)
	dig_node_add_updated(plus, node);

    if (type & GV_LINES) {
	lp = Points->n_points - 1;
	G_debug(3, "Register node %f,%f", Points->x[lp], Points->y[lp]);
	node =
	    dig_find_node(plus, Points->x[lp], Points->y[lp], Points->z[lp]);
	G_debug(3, "node = %d", node);
	if (node == 0) {
	    node =
		dig_add_node(plus, Points->x[lp], Points->y[lp],
			     Points->z[lp]);
	    G_debug(3, "Add new node: %d", node);
	}
	else {
	    G_debug(3, "Old node found: %d", node);
	}
	line->N2 = node;
	dig_node_add_line(plus, node, -lineid, Points, type);
	if (plus->do_uplist)
	    dig_node_add_updated(plus, node);
    }
    else {
	line->N2 = 0;
    }

    line->type = type;
    line->offset = offset;
    line->left = 0;
    line->right = 0;
    line->N = 0;
    line->S = 0;
    line->E = 0;
    line->W = 0;

    dig_line_box(Points, &box);
    dig_line_set_box(plus, lineid, &box);
    dig_spidx_add_line(plus, lineid, &box);
    if (plus->do_uplist)
	dig_line_add_updated(plus, lineid);

    return (lineid);
}

/*!
 * \brief Add new line to Plus_head structure.
 *
 * \param[in,out] plus pointer to Plus_head structure
 * \param[in] type feature type
 * \param[in] Points line geometry
 * \param[in] offset line offset
 *
 * \return -1 on error      
 * \return line id
 */
int
dig_add_line(struct Plus_head *plus, int type, struct line_pnts *Points,
	     long offset)
{
    int ret;
    
    /* First look if we have space in array of pointers to lines
     *  and reallocate if necessary */
    if (plus->n_lines >= plus->alloc_lines) {	/* array is full */
	if (dig_alloc_lines(plus, 1000) == -1)
	    return -1;
    }

    ret = add_line(plus, plus->n_lines + 1, type, Points, offset);

    if (ret == -1)
	return ret;

    plus->n_lines++;

    switch (type) {
    case GV_POINT:
	plus->n_plines++;
	break;
    case GV_LINE:
	plus->n_llines++;
	break;
    case GV_BOUNDARY:
	plus->n_blines++;
	break;
    case GV_CENTROID:
	plus->n_clines++;
	break;
    case GV_FACE:
	plus->n_flines++;
	break;
    case GV_KERNEL:
	plus->n_klines++;
	break;
    }

    return ret;
}

/*!
 * \brief Restore line in Plus_head structure.
 *
 * \param[in,out] plus pointer to Plus_head structure
 * \param[in] type feature type
 * \param[in] Points line geometry
 * \param[in] offset line offset
 *
 * \return -1 on error      
 * \return line id
 */
int
dig_restore_line(struct Plus_head *plus, int lineid,
		 int type, struct line_pnts *Points,
		 long offset)
{
    if (lineid < 1 || lineid > plus->n_lines) {
	return -1;
    }

    return add_line(plus, lineid, type, Points, offset);    
}

/*!
 * \brief Delete line from Plus_head structure.
 *
 * Doesn't update area/isle references (dig_del_area() or dig_del_isle()) must be
 * run before the line is deleted if the line is part of such
 * structure). Update is info about line in nodes. If this line is
 * last in node then node is deleted.
 *
 * \param[in,out] plus pointer to Plus_head structure
 * \param[in] line line id
 *
 * \return -1 on error
 * \return  0 OK
 *
 */
int dig_del_line(struct Plus_head *plus, int line)
{
    int i, mv;
    P_LINE *Line;
    P_NODE *Node;

    /* TODO: free structures */
    G_debug(3, "dig_del_line() line =  %d", line);

    Line = plus->Line[line];
    dig_spidx_del_line(plus, line);

    /* Delete from nodes (and nodes) */
    Node = plus->Node[Line->N1];
    mv = 0;
    for (i = 0; i < Node->n_lines; i++) {
	if (mv) {
	    Node->lines[i - 1] = Node->lines[i];
	    Node->angles[i - 1] = Node->angles[i];
	}
	else {
	    if (abs(Node->lines[i]) == line)
		mv = 1;
	}
    }
    Node->n_lines--;
    if (Node->n_lines == 0) {
	G_debug(3, "    node %d has 0 lines -> delete", Line->N1);
	dig_spidx_del_node(plus, Line->N1);
	plus->Node[Line->N1] = NULL;
    }
    else {
	if (plus->do_uplist)
	    dig_node_add_updated(plus, Line->N1);
    }

    if (Line->type & GV_LINES) {
	Node = plus->Node[Line->N2];
	mv = 0;
	for (i = 0; i < Node->n_lines; i++) {
	    if (mv) {
		Node->lines[i - 1] = Node->lines[i];
		Node->angles[i - 1] = Node->angles[i];
	    }
	    else {
		if (abs(Node->lines[i]) == line)
		    mv = 1;
	    }
	}
	Node->n_lines--;
	if (Node->n_lines == 0) {
	    G_debug(3, "    node %d has 0 lines -> delete", Line->N2);
	    dig_spidx_del_node(plus, Line->N2);
	    plus->Node[Line->N2] = NULL;
	}
	else {
	    if (plus->do_uplist)
		dig_node_add_updated(plus, Line->N2);
	}
    }

    /* Delete line */
    plus->Line[line] = NULL;

    return 0;
}

/*!
 * \brief Get area number on line side.
 *
 * \param[in] plus pointer Plus_head structure
 * \param[in] line line id
 * \param[in] side side id (GV_LEFT || GV_RIGHT)
 *
 * \return  area number 
 * \return  0 no area
 * \return -1 on error
 */
plus_t dig_line_get_area(struct Plus_head * plus, plus_t line, int side)
{
    P_LINE *Line;

    Line = plus->Line[line];
    if (side == GV_LEFT) {
	G_debug(3,
		"dig_line_get_area(): line = %d, side = %d (left), area = %d",
		line, side, Line->left);
	return (Line->left);
    }
    if (side == GV_RIGHT) {
	G_debug(3,
		"dig_line_get_area(): line = %d, side = %d (right), area = %d",
		line, side, Line->right);

	return (Line->right);
    }

    return (-1);
}

/*!
 * \brief Set area number on line side
 *
 * \param[in] plus pointer Plus_head structure
 * \param[in] line line id
 * \param[in] side side id (GV_LEFT || GV_RIGHT)
 * \param[in] area area id
 *
 * \return 1
 */
int
dig_line_set_area(struct Plus_head *plus, plus_t line, int side, plus_t area)
{
    P_LINE *Line;

    Line = plus->Line[line];
    if (side == GV_LEFT) {
	Line->left = area;
    }
    else if (side == GV_RIGHT) {
	Line->right = area;
    }

    return (1);
}

/*!
 * \brief Set line bounding box
 *
 * \param[in] plus pointer Plus_head structure
 * \param[in] line line id
 * \param[in] Box  bounding box
 *
 * \return 1
 */
int dig_line_set_box(struct Plus_head *plus, plus_t line, BOUND_BOX * Box)
{
    P_LINE *Line;

    Line = plus->Line[line];

    Line->N = Box->N;
    Line->S = Box->S;
    Line->E = Box->E;
    Line->W = Box->W;
    Line->T = Box->T;
    Line->B = Box->B;

    return (1);
}

/*!
 * \brief Get line bounding box saved in topo
 * 
 * \param[in] plus pointer Plus_head structure
 * \param[in] line line id
 * \param[in,out] Box bounding box
 *
 * \return 1
 */
int dig_line_get_box(struct Plus_head *plus, plus_t line, BOUND_BOX * Box)
{
    P_LINE *Line;

    Line = plus->Line[line];

    Box->N = Line->N;
    Box->S = Line->S;
    Box->E = Line->E;
    Box->W = Line->W;
    Box->T = Line->T;
    Box->B = Line->B;

    return (1);
}
