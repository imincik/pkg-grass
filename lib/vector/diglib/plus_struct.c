/*
 ****************************************************************************
 *
 * MODULE:       Vector library 
 *              
 * AUTHOR(S):    Dave Gerdes, CERL.
 *               Update to GRASS 5.7 Radim Blazek.
 *
 * PURPOSE:      Lower level functions for reading/writing/manipulating vectors.
 *
 * COPYRIGHT:    (C) 2001 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *              License (>=v2). Read the file COPYING that comes with GRASS
 *              for details.
 *
 *****************************************************************************/
#include <string.h>
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/version.h>

/*
 * Routines for reading and writing Dig+ structures.
 * return 0 on success, -1 on failure of whatever kind
 * if you dont want it written out, then dont call these routines
 * ie  check for deleted status before calling a write routine
 * in as much as it would be nice to hide that code in here,
 * this is a library routine and we chose to make it dependent on
 * as few external files as possible 
 */

/*  These routines assume ptr->alloc_lines  is valid 
 *   Make sure it is initialized before calling 
 */

/*
 *  Internally, my default variables for lines/areas/nodes/isles  are type
 *  plus_t  which is typedefed as short.  This limits the current version
 *  to no more than 32K lines, nodes etc. (excluding points)
 *  All in the name of future expansion, I have converted these values to 
 *  longs in the dig_plus data file.
 *
 *  NOTE: 3.10 changes plus_t to  ints.
 *    This assumes that any reasonable machine will use 4 bytes to
 *    store an int.  The mapdev code is not guaranteed to work if
 *    plus_t is changed to a type that is larger than an int.
 */

int dig_Rd_P_node(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int cnt, n_edges;
    P_NODE *ptr;

    G_debug(3, "dig_Rd_P_node()");


    if (0 >= dig__fread_port_P(&cnt, 1, fp))
	return (-1);

    if (cnt == 0) {		/* dead */
	G_debug(3, "    node is dead");
	Plus->Node[n] = NULL;
	return 0;
    }

    ptr = dig_alloc_node();
    ptr->n_lines = cnt;

    if (dig_node_alloc_line(ptr, ptr->n_lines) == -1)
	return -1;

    if (ptr->n_lines) {
	if (0 >= dig__fread_port_P(ptr->lines, ptr->n_lines, fp))
	    return (-1);
	if (0 >= dig__fread_port_F(ptr->angles, ptr->n_lines, fp))
	    return (-1);
    }

    if (Plus->with_z)
	if (0 >= dig__fread_port_P(&n_edges, 1, fp))	/* reserved for edges */
	    return (-1);

    /* here will be edges */

    if (0 >= dig__fread_port_D(&(ptr->x), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->y), 1, fp))
	return (-1);

    if (Plus->with_z) {
	if (0 >= dig__fread_port_D(&(ptr->z), 1, fp))
	    return (-1);
    }
    else
	ptr->z = 0;


    Plus->Node[n] = ptr;

    return (0);
}

int dig_Wr_P_node(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int i, n_edges = 0;
    P_NODE *ptr;

    G_debug(3, "dig_Wr_P_node()");
    ptr = Plus->Node[n];

    /* If NULL i.e. dead write just 0 instead of number of lines */
    if (ptr == NULL) {
	G_debug(3, "    node is dead -> write 0 only");
	i = 0;
	if (0 >= dig__fwrite_port_P(&i, 1, fp))
	    return (-1);
	return 0;
    }

    if (0 >= dig__fwrite_port_P(&(ptr->n_lines), 1, fp))
	return (-1);

    if (ptr->n_lines) {
	if (0 >= dig__fwrite_port_P(ptr->lines, ptr->n_lines, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_F(ptr->angles, ptr->n_lines, fp))
	    return (-1);
    }

    if (Plus->with_z)
	if (0 >= dig__fwrite_port_P(&n_edges, 1, fp))	/* reserved for edges */
	    return (-1);

    /* here will be edges */

    if (0 >= dig__fwrite_port_D(&(ptr->x), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->y), 1, fp))
	return (-1);

    if (Plus->with_z)
	if (0 >= dig__fwrite_port_D(&(ptr->z), 1, fp))
	    return (-1);

    return (0);
}

int dig_Rd_P_line(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int n_edges, vol;
    char tp;
    P_LINE *ptr;
    P_NODE *Node;

    G_debug(3, "dig_Rd_P_line()");

    if (0 >= dig__fread_port_C(&tp, 1, fp))
	return (-1);

    if (tp == 0) {		/* dead */
	G_debug(3, "    line is dead");
	Plus->Line[n] = NULL;
	return 0;
    }

    ptr = dig_alloc_line();

    ptr->type = dig_type_from_store(tp);
    G_debug(5, "    line type  %d -> %d", tp, ptr->type);

    if (0 >= dig__fread_port_L(&(ptr->offset), 1, fp))
	return (-1);

    /* First node */
    if (ptr->type & (GV_POINTS | GV_LINES | GV_KERNEL))
	if (0 >= dig__fread_port_P(&(ptr->N1), 1, fp))
	    return -1;

    /* Second node, for points/centroids not needed */
    if (ptr->type & (GV_LINE | GV_BOUNDARY)) {
	if (0 >= dig__fread_port_P(&(ptr->N2), 1, fp))
	    return -1;
    }
    else if (ptr->type & (GV_POINTS | GV_KERNEL))
	ptr->N2 = ptr->N1;

    /* left area for boundary, area for centroid */
    if (ptr->type & (GV_BOUNDARY | GV_CENTROID))
	if (0 >= dig__fread_port_P(&(ptr->left), 1, fp))
	    return -1;

    /* right area */
    if (ptr->type & GV_BOUNDARY)
	if (0 >= dig__fread_port_P(&(ptr->right), 1, fp))
	    return -1;

    if ((ptr->type & GV_FACE) && Plus->with_z) {	/* reserved for face edges */
	if (0 >= dig__fread_port_I(&n_edges, 1, fp))
	    return -1;

	/* here will be list of edges */

	/* left / right volume */
	if (0 >= dig__fread_port_P(&vol, 1, fp))
	    return -1;
	if (0 >= dig__fread_port_P(&vol, 1, fp))
	    return -1;
    }

    if ((ptr->type & GV_KERNEL) && Plus->with_z)	/* reserved for kernel (volume number) */
	if (0 >= dig__fread_port_P(&vol, 1, fp))
	    return -1;

    /* Bounding box */
    if (ptr->type & (GV_LINE | GV_BOUNDARY | GV_FACE)) {
	if (0 >= dig__fread_port_D(&(ptr->N), 1, fp))
	    return -1;
	if (0 >= dig__fread_port_D(&(ptr->S), 1, fp))
	    return -1;
	if (0 >= dig__fread_port_D(&(ptr->E), 1, fp))
	    return -1;
	if (0 >= dig__fread_port_D(&(ptr->W), 1, fp))
	    return -1;

	if (Plus->with_z) {
	    if (0 >= dig__fread_port_D(&(ptr->T), 1, fp))
		return -1;
	    if (0 >= dig__fread_port_D(&(ptr->B), 1, fp))
		return -1;
	}
	else {
	    ptr->T = 0.0;
	    ptr->B = 0.0;
	}
    }
    else {
	Node = Plus->Node[ptr->N1];
	ptr->N = Node->y;
	ptr->S = Node->y;
	ptr->E = Node->x;
	ptr->W = Node->x;
	ptr->T = Node->z;
	ptr->B = Node->z;
    }

    Plus->Line[n] = ptr;
    return (0);
}

int dig_Wr_P_line(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int n_edges = 0, vol = 0;
    char ch;
    P_LINE *ptr;

    G_debug(4, "dig_Wr_P_line() line = %d", n);

    ptr = Plus->Line[n];

    /* If NULL i.e. dead write just 0 instead of type */
    if (ptr == NULL) {
	G_debug(3, "    line is dead -> write 0 only");
	ch = 0;
	if (0 >= dig__fwrite_port_C(&ch, 1, fp))
	    return (-1);
	return 0;
    }

    ch = (char)dig_type_to_store(ptr->type);
    G_debug(5, "    line type  %d -> %d", ptr->type, ch);
    if (0 >= dig__fwrite_port_C(&ch, 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->offset), 1, fp))
	return (-1);

    /* First node */
    if (ptr->type & (GV_POINTS | GV_LINES | GV_KERNEL))
	if (0 >= dig__fwrite_port_P(&(ptr->N1), 1, fp))
	    return (-1);

    /* Second node, for points/centroids not needed */
    if (ptr->type & (GV_LINE | GV_BOUNDARY))
	if (0 >= dig__fwrite_port_P(&(ptr->N2), 1, fp))
	    return (-1);

    /* left area for boundary, area for centroid */
    if (ptr->type & (GV_BOUNDARY | GV_CENTROID))
	if (0 >= dig__fwrite_port_P(&(ptr->left), 1, fp))
	    return (-1);

    /* right area */
    if (ptr->type & GV_BOUNDARY)
	if (0 >= dig__fwrite_port_P(&(ptr->right), 1, fp))
	    return (-1);

    if ((ptr->type & GV_FACE) && Plus->with_z) {	/* reserved for face */
	if (0 >= dig__fwrite_port_I(&n_edges, 1, fp))
	    return (-1);

	/* here will be list of edges */

	/* left / right volume */
	if (0 >= dig__fwrite_port_P(&vol, 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_P(&vol, 1, fp))
	    return (-1);
    }

    if ((ptr->type & GV_KERNEL) && Plus->with_z)	/* reserved for kernel (volume number) */
	if (0 >= dig__fwrite_port_P(&vol, 1, fp))
	    return (-1);

    /* Bounding box */
    if (ptr->type & (GV_LINE | GV_BOUNDARY | GV_FACE)) {
	if (0 >= dig__fwrite_port_D(&(ptr->N), 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_D(&(ptr->S), 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_D(&(ptr->E), 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_D(&(ptr->W), 1, fp))
	    return (-1);

	if (Plus->with_z) {
	    if (0 >= dig__fwrite_port_D(&(ptr->T), 1, fp))
		return (-1);
	    if (0 >= dig__fwrite_port_D(&(ptr->B), 1, fp))
		return (-1);
	}
    }

    return (0);
}

int dig_Rd_P_area(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int cnt;
    P_AREA *ptr;

#ifdef GDEBUG
    G_debug(3, "dig_Rd_P_area(): n = %d", n);
#endif

    if (0 >= dig__fread_port_P(&cnt, 1, fp))
	return (-1);

    if (cnt == 0) {		/* dead */
	Plus->Area[n] = NULL;
	return 0;
    }

    ptr = dig_alloc_area();

    /* lines */
    ptr->n_lines = cnt;

    if (dig_area_alloc_line(ptr, ptr->n_lines) == -1)
	return -1;

    if (ptr->n_lines)
	if (0 >= dig__fread_port_P(ptr->lines, ptr->n_lines, fp))
	    return -1;

    /* isles */
    if (0 >= dig__fread_port_P(&(ptr->n_isles), 1, fp))
	return -1;

    if (dig_area_alloc_isle(ptr, ptr->n_isles) == -1)
	return -1;

    if (ptr->n_isles)
	if (0 >= dig__fread_port_P(ptr->isles, ptr->n_isles, fp))
	    return -1;

    /* centroid */
    if (0 >= dig__fread_port_P(&(ptr->centroid), 1, fp))
	return -1;

    if (0 >= dig__fread_port_D(&(ptr->N), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->S), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->E), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->W), 1, fp))
	return -1;

    if (Plus->with_z) {
	if (0 >= dig__fread_port_D(&(ptr->T), 1, fp))
	    return -1;
	if (0 >= dig__fread_port_D(&(ptr->B), 1, fp))
	    return -1;
    }
    else {
	ptr->T = 0.0;
	ptr->B = 0.0;
    }

    Plus->Area[n] = ptr;

    return (0);
}

int dig_Wr_P_area(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int i;
    P_AREA *ptr;

    ptr = Plus->Area[n];

    /* If NULL i.e. dead write just 0 instead of number of lines */
    if (ptr == NULL) {
	i = 0;
	if (0 >= dig__fwrite_port_P(&i, 1, fp))
	    return (-1);
	return 0;
    }

    /* lines */
    if (0 >= dig__fwrite_port_P(&(ptr->n_lines), 1, fp))
	return (-1);

    if (ptr->n_lines)
	if (0 >= dig__fwrite_port_P(ptr->lines, ptr->n_lines, fp))
	    return -1;

    /* isles */
    if (0 >= dig__fwrite_port_P(&(ptr->n_isles), 1, fp))
	return (-1);

    if (ptr->n_isles)
	if (0 >= dig__fwrite_port_P(ptr->isles, ptr->n_isles, fp))
	    return -1;

    /* centroid */
    if (0 >= dig__fwrite_port_P(&(ptr->centroid), 1, fp))
	return (-1);

    if (0 >= dig__fwrite_port_D(&(ptr->N), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->S), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->E), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->W), 1, fp))
	return (-1);

    if (Plus->with_z) {
	if (0 >= dig__fwrite_port_D(&(ptr->T), 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_D(&(ptr->B), 1, fp))
	    return (-1);
    }

    return (0);
}

int dig_Rd_P_isle(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int cnt;
    P_ISLE *ptr;

#ifdef GDEBUG
    G_debug(3, "dig_Rd_P_isle()");
#endif

    if (0 >= dig__fread_port_P(&cnt, 1, fp))
	return (-1);

    if (cnt == 0) {		/* dead */
	Plus->Isle[n] = NULL;
	return 0;
    }

    ptr = dig_alloc_isle();

    /* lines */
    ptr->n_lines = cnt;

    if (dig_isle_alloc_line(ptr, ptr->n_lines) == -1)
	return -1;

    if (ptr->n_lines)
	if (0 >= dig__fread_port_P(ptr->lines, ptr->n_lines, fp))
	    return -1;

    /* area */
    if (0 >= dig__fread_port_P(&(ptr->area), 1, fp))
	return -1;

    if (0 >= dig__fread_port_D(&(ptr->N), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->S), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->E), 1, fp))
	return -1;
    if (0 >= dig__fread_port_D(&(ptr->W), 1, fp))
	return -1;

    if (Plus->with_z) {
	if (0 >= dig__fread_port_D(&(ptr->T), 1, fp))
	    return -1;
	if (0 >= dig__fread_port_D(&(ptr->B), 1, fp))
	    return -1;
    }
    else {
	ptr->T = 0.0;
	ptr->B = 0.0;
    }

    Plus->Isle[n] = ptr;

    return (0);
}

int dig_Wr_P_isle(struct Plus_head *Plus, int n, GVFILE * fp)
{
    int i;
    P_ISLE *ptr;

    ptr = Plus->Isle[n];

    /* If NULL i.e. dead write just 0 instead of number of lines */
    if (ptr == NULL) {
	i = 0;
	if (0 >= dig__fwrite_port_P(&i, 1, fp))
	    return (-1);
	return 0;
    }

    /* lines */
    if (0 >= dig__fwrite_port_P(&(ptr->n_lines), 1, fp))
	return (-1);

    if (ptr->n_lines)
	if (0 >= dig__fwrite_port_P(ptr->lines, ptr->n_lines, fp))
	    return -1;

    /* area */
    if (0 >= dig__fwrite_port_P(&(ptr->area), 1, fp))
	return (-1);

    if (0 >= dig__fwrite_port_D(&(ptr->N), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->S), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->E), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->W), 1, fp))
	return (-1);

    if (Plus->with_z) {
	if (0 >= dig__fwrite_port_D(&(ptr->T), 1, fp))
	    return (-1);
	if (0 >= dig__fwrite_port_D(&(ptr->B), 1, fp))
	    return (-1);
    }

    return (0);
}

/*
   \return -1 error
   \return  0 OK 
 */
int dig_Rd_Plus_head(GVFILE * fp, struct Plus_head *ptr)
{
    unsigned char buf[5];
    int byte_order;

    dig_rewind(fp);

    /* bytes 1 - 5 */
    if (0 >= dig__fread_port_C(buf, 5, fp))
	return (-1);
    ptr->Version_Major = buf[0];
    ptr->Version_Minor = buf[1];
    ptr->Back_Major = buf[2];
    ptr->Back_Minor = buf[3];
    byte_order = buf[4];

    G_debug(2,
	    "Topo header: file version %d.%d , supported from GRASS version %d.%d",
	    ptr->Version_Major, ptr->Version_Minor, ptr->Back_Major,
	    ptr->Back_Minor);

    G_debug(2, "  byte order %d", byte_order);

    /* check version numbers */
    if (ptr->Version_Major > GV_TOPO_VER_MAJOR ||
	ptr->Version_Minor > GV_TOPO_VER_MINOR) {
	/* The file was created by GRASS library with higher version than this one */

	if (ptr->Back_Major > GV_TOPO_VER_MAJOR ||
	    ptr->Back_Minor > GV_TOPO_VER_MINOR) {
	    /* This version of GRASS lib is lower than the oldest which can read this format */
	    G_debug(1, "Topology format version %d.%d",
		    ptr->Version_Major, ptr->Version_Minor);
	    G_fatal_error
		("This version of GRASS (%d.%d) is too old to read this topology format."
		 " Try to rebuild topology or upgrade GRASS to at least version %d.",
		 GRASS_VERSION_MAJOR, GRASS_VERSION_MINOR, GRASS_VERSION_MAJOR + 1);
	    return (-1);
	}

	G_warning
	    ("Your GRASS version does not fully support topology format %d.%d of the vector."
	     " Consider to rebuild topology or upgrade GRASS.",
	     ptr->Version_Major, ptr->Version_Minor);
    }

    dig_init_portable(&(ptr->port), byte_order);
    dig_set_cur_port(&(ptr->port));

    /* bytes 6 - 9 : header size */
    if (0 >= dig__fread_port_L(&(ptr->head_size), 1, fp))
	return (-1);
    G_debug(2, "  header size %ld", ptr->head_size);

    /* byte 10 : dimension 2D or 3D */
    if (0 >= dig__fread_port_C(buf, 1, fp))
	return (-1);
    ptr->with_z = buf[0];
    G_debug(2, "  with_z %d", ptr->with_z);

    /* bytes 11 - 58 : bound box */
    if (0 >= dig__fread_port_D(&(ptr->box.N), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->box.S), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->box.E), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->box.W), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->box.T), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_D(&(ptr->box.B), 1, fp))
	return (-1);

    /* bytes 59 - 86 : number of structures */
    if (0 >= dig__fread_port_P(&(ptr->n_nodes), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_edges), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_lines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_areas), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_isles), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_volumes), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_holes), 1, fp))
	return (-1);

    /* bytes 87 - 110 : number of line types */
    if (0 >= dig__fread_port_P(&(ptr->n_plines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_llines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_blines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_clines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_flines), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_P(&(ptr->n_klines), 1, fp))
	return (-1);

    /* bytes 111 - 138 : Offset */
    if (0 >= dig__fread_port_L(&(ptr->Node_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Edge_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Line_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Area_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Isle_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Volume_offset), 1, fp))
	return (-1);
    if (0 >= dig__fread_port_L(&(ptr->Hole_offset), 1, fp))
	return (-1);

    /* bytes 139 - 142 : Coor size and time */
    if (0 >= dig__fread_port_L(&(ptr->coor_size), 1, fp))
	return (-1);

    G_debug(2, "  coor size %ld", ptr->coor_size);

    dig_fseek(fp, ptr->head_size, SEEK_SET);

    return (0);
}

int dig_Wr_Plus_head(GVFILE * fp, struct Plus_head *ptr)
{
    unsigned char buf[10];
    long length = 142;

    dig_rewind(fp);
    dig_set_cur_port(&(ptr->port));

    /* bytes 1 - 5 */
    buf[0] = GV_TOPO_VER_MAJOR;
    buf[1] = GV_TOPO_VER_MINOR;
    buf[2] = GV_TOPO_EARLIEST_MAJOR;
    buf[3] = GV_TOPO_EARLIEST_MINOR;
    buf[4] = ptr->port.byte_order;
    if (0 >= dig__fwrite_port_C(buf, 5, fp))
	return (-1);

    /* bytes 6 - 9 : header size */
    if (0 >= dig__fwrite_port_L(&length, 1, fp))
	return (0);

    /* byte 10 : dimension 2D or 3D */
    buf[0] = ptr->with_z;
    if (0 >= dig__fwrite_port_C(buf, 1, fp))
	return (0);

    /* bytes 11 - 58 : bound box */
    if (0 >= dig__fwrite_port_D(&(ptr->box.N), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->box.S), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->box.E), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->box.W), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->box.T), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_D(&(ptr->box.B), 1, fp))
	return (-1);

    /* bytes 59 - 86 : number of structures */
    if (0 >= dig__fwrite_port_P(&(ptr->n_nodes), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_edges), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_lines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_areas), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_isles), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_volumes), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_holes), 1, fp))
	return (-1);

    /* bytes 87 - 110 : number of line types */
    if (0 >= dig__fwrite_port_P(&(ptr->n_plines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_llines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_blines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_clines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_flines), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_P(&(ptr->n_klines), 1, fp))
	return (-1);

    /* bytes 111 - 138 : Offset */
    if (0 >= dig__fwrite_port_L(&(ptr->Node_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Edge_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Line_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Area_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Isle_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Volume_offset), 1, fp))
	return (-1);
    if (0 >= dig__fwrite_port_L(&(ptr->Hole_offset), 1, fp))
	return (-1);

    /* bytes 139 - 142 : Coor size and time */
    if (0 >= dig__fwrite_port_L(&(ptr->coor_size), 1, fp))
	return (-1);

    G_debug(2, "topo body offset %ld", dig_ftell(fp));

    return (0);
}
