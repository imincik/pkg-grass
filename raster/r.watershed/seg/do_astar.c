#include <stdlib.h>
#include <unistd.h>
#include <grass/gis.h>
#include <grass/glocale.h>
#include "Gwater.h"
#include "do_astar.h"

double get_slope2(CELL, CELL, double);

int do_astar(void)
{
    POINT point;
    int doer, count;
    SHORT upr, upc, r, c, ct_dir;
    CELL work_val, alt_val, alt_nbr[8], alt_up, asp_up;
    DCELL wat_val;
    CELL in_val, drain_val;
    HEAP heap_pos;
    /* sides
     * |7|1|4|
     * |2| |3|
     * |5|0|6|
     */
    int nbr_ew[8] = { 0, 1, 2, 3, 1, 0, 0, 1 };
    int nbr_ns[8] = { 0, 1, 2, 3, 3, 2, 3, 2 };
    double dx, dy, dist_to_nbr[8], ew_res, ns_res;
    double slope[8];
    int skip_diag;

    G_message(_("SECTION 2: A * Search."));

    for (ct_dir = 0; ct_dir < sides; ct_dir++) {
	/* get r, c (r_nbr, c_nbr) for neighbours */
	upr = nextdr[ct_dir];
	upc = nextdc[ct_dir];
	/* account for rare cases when ns_res != ew_res */
	dy = ABS(upr) * window.ns_res;
	dx = ABS(upc) * window.ew_res;
	if (ct_dir < 4)
	    dist_to_nbr[ct_dir] = dx + dy;
	else
	    dist_to_nbr[ct_dir] = sqrt(dx * dx + dy * dy);
    }
    ew_res = window.ew_res;
    ns_res = window.ns_res;

    count = 0;
    seg_get(&heap_index, (char *)&heap_pos, 0, 1);
    first_astar = heap_pos.point;

    /* A * Search: downhill path through all not masked cells */
    while (first_astar != -1) {
	G_percent(count++, do_points, 1);

	seg_get(&heap_index, (char *)&heap_pos, 0, 1);
	doer = heap_pos.point;

	seg_get(&astar_pts, (char *)&point, 0, doer);

	alt_val = heap_pos.ele;

	/* drop astar_pts[doer] from heap */
	drop_pt();

	seg_get(&heap_index, (char *)&heap_pos, 0, 1);
	first_astar = heap_pos.point;

	point.nxt = first_cum;
	seg_put(&astar_pts, (char *)&point, 0, doer);

	first_cum = doer;
	r = point.r;
	c = point.c;

	bseg_put(&worked, &one, r, c);

	/* check all neighbours, breadth first search */
	for (ct_dir = 0; ct_dir < sides; ct_dir++) {
	    /* get r, c (upr, upc) for this neighbour */
	    upr = r + nextdr[ct_dir];
	    upc = c + nextdc[ct_dir];
	    slope[ct_dir] = alt_nbr[ct_dir] = 0;
	    /* check that upr, upc are within region */
	    if (upr >= 0 && upr < nrows && upc >= 0 && upc < ncols) {
		/* avoid diagonal flow direction bias */
		bseg_get(&in_list, &in_val, upr, upc);
		bseg_get(&worked, &work_val, upr, upc);
		skip_diag = 0;
		if (!work_val) {
		    cseg_get(&alt, &alt_up, upr, upc);
		    alt_nbr[ct_dir] = alt_up;
		    slope[ct_dir] =
			get_slope2(alt_val, alt_nbr[ct_dir],
				   dist_to_nbr[ct_dir]);
		}
		if (!in_val) {
		    if (ct_dir > 3 && slope[ct_dir] > 0) {
			if (slope[nbr_ew[ct_dir]] > 0) {
			    /* slope to ew nbr > slope to center */
			    if (slope[ct_dir] <
				get_slope2(alt_nbr[nbr_ew[ct_dir]],
					   alt_nbr[ct_dir], ew_res))
				skip_diag = 1;
			}
			if (!skip_diag && slope[nbr_ns[ct_dir]] > 0) {
			    /* slope to ns nbr > slope to center */
			    if (slope[ct_dir] <
				get_slope2(alt_nbr[nbr_ns[ct_dir]],
					   alt_nbr[ct_dir], ns_res))
				skip_diag = 1;
			}
		    }
		}
		/* put neighbour in search list if not yet in */
		if (in_val == 0 && skip_diag == 0) {
		    add_pt(upr, upc, alt_nbr[ct_dir], alt_val);
		    /* flow direction is set here */
		    drain_val = drain[upr - r + 1][upc - c + 1];
		    cseg_put(&asp, &drain_val, upr, upc);
		}
		/* check if neighbour has not been worked on */
		else if (in_val && !work_val) {
		    cseg_get(&asp, &asp_up, upr, upc);
		    if (asp_up < 0) {
			drain_val = drain[upr - r + 1][upc - c + 1];
			cseg_put(&asp, &drain_val, upr, upc);
			dseg_get(&wat, &wat_val, r, c);
			if (wat_val > 0) {
			    wat_val = -wat_val;
			    dseg_put(&wat, &wat_val, r, c);
			}
		    }
		}
	    }
	}    /* end sides */
    }

    if (mfd == 0)
	bseg_close(&worked);

    bseg_close(&in_list);
    seg_close(&heap_index);

    G_percent(count, do_points, 1);	/* finish it */
    return 0;
}

/* new add point routine for min heap */
int add_pt(SHORT r, SHORT c, CELL ele, CELL downe)
{
    POINT point;
    HEAP heap_pos;

    bseg_put(&in_list, &one, r, c);

    /* add point to next free position */

    heap_size++;

    if (heap_size > do_points)
	G_fatal_error(_("heapsize too large"));

    heap_pos.point = nxt_avail_pt;
    heap_pos.ele = ele;
    seg_put(&heap_index, (char *)&heap_pos, 0, heap_size);

    point.r = r;
    point.c = c;

    seg_put(&astar_pts, (char *)&point, 0, nxt_avail_pt);

    /* cseg_put(&pnt_index, &nxt_avail_pt, r, c); */

    nxt_avail_pt++;

    /* sift up: move new point towards top of heap */

    sift_up(heap_size, ele);

    return 0;
}

/* drop point routine for min heap */
int drop_pt(void)
{
    int child, childr, parent;
    int childp;
    CELL ele;
    int i;
    HEAP heap_pos;

    if (heap_size == 1) {
	parent = -1;
	heap_pos.point = -1;
	heap_pos.ele = 0;
	seg_put(&heap_index, (char *)&heap_pos, 0, 1);
	heap_size = 0;
	return 0;
    }

    /* start with heap root */
    parent = 1;

    /* sift down: move hole back towards bottom of heap */
    /* sift-down routine customised for A * Search logic */

    while ((child = GET_CHILD(parent)) <= heap_size) {
	/* select child with lower ele, if both are equal, older child
	 * older child is older startpoint for flow path, important */
	seg_get(&heap_index, (char *)&heap_pos, 0, child);
	childp = heap_pos.point;
	ele = heap_pos.ele;
	if (child < heap_size) {
	    childr = child + 1;
	    i = child + 3;
	    while (childr <= heap_size && childr < i) {
		seg_get(&heap_index, (char *)&heap_pos, 0, childr);
		if (heap_pos.ele < ele) {
		    child = childr;
		    childp = heap_pos.point;
		    ele = heap_pos.ele;
		}
		/* make sure we get the oldest child */
		else if (ele == heap_pos.ele && childp > heap_pos.point) {
		    child = childr;
		    childp = heap_pos.point;
		}
		childr++;
	    }
	}

	/* move hole down */
	heap_pos.point = childp;
	heap_pos.ele = ele;
	seg_put(&heap_index, (char *)&heap_pos, 0, parent);
	parent = child;

    }

    /* hole is in lowest layer, move to heap end */
    if (parent < heap_size) {
	seg_get(&heap_index, (char *)&heap_pos, 0, heap_size);
	seg_put(&heap_index, (char *)&heap_pos, 0, parent);

	ele = heap_pos.ele;

	/* sift up last swapped point, only necessary if hole moved to heap end */
	sift_up(parent, ele);
    }

    /* the actual drop */
    heap_size--;

    return 0;

}

/* standard sift-up routine for d-ary min heap */
int sift_up(int start, CELL ele)
{
    int parent, child, childp;
    HEAP heap_pos;

    child = start;
    seg_get(&heap_index, (char *)&heap_pos, 0, child);
    childp = heap_pos.point;

    while (child > 1) {
	parent = GET_PARENT(child);
	seg_get(&heap_index, (char *)&heap_pos, 0, parent);

	/* parent ele higher */
	if (heap_pos.ele > ele) {

	    /* push parent point down */
	    seg_put(&heap_index, (char *)&heap_pos, 0, child);
	    child = parent;
	}
	/* same ele, but parent is younger */
	else if (heap_pos.ele == ele && heap_pos.point > childp) {

	    /* push parent point down */
	    seg_put(&heap_index, (char *)&heap_pos, 0, child);
	    child = parent;
	}
	else
	    /* no more sifting up, found new slot for child */
	    break;
    }

    /* no more sifting up, found new slot for child */
    if (child < start) {
	heap_pos.point = childp;
	heap_pos.ele = ele;
	seg_put(&heap_index, (char *)&heap_pos, 0, child);
    }
    return 0;
}

double
get_slope(SHORT r, SHORT c, SHORT downr, SHORT downc, CELL ele, CELL downe)
{
    double slope;

    if (r == downr)
	slope = (ele - downe) / window.ew_res;
    else if (c == downc)
	slope = (ele - downe) / window.ns_res;
    else
	slope = (ele - downe) / diag;
    if (slope < MIN_SLOPE)
	return (MIN_SLOPE);
    return (slope);
}

double get_slope2(CELL ele, CELL up_ele, double dist)
{
    if (ele >= up_ele)
	return 0.0;
    else
	return (double)(up_ele - ele) / dist;
}

/* no longer needed */
int replace(SHORT upr, SHORT upc, SHORT r, SHORT c)
{				/* ele was in there */
    /* CELL ele;  */
    int now;
    POINT point;

    now = 0;

    /* cseg_get(&pnt_index, &now, upr, upc); */
    seg_get(&astar_pts, (char *)&point, 0, now);
    if (point.r != upr || point.c != upc) {
	G_warning("pnt_index incorrect!");
	return 1;
    }
    /* point.downr = r;
    point.downc = c; */
    seg_put(&astar_pts, (char *)&point, 0, now);

    return 0;
}
