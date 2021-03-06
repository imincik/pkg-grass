#include "Gwater.h"

int slope_length(SHORT r, SHORT c, SHORT dr, SHORT dc)
{
    CELL top_alt, bot_alt, asp_value, ridge;
    double res, top_ls, bot_ls;

    if (sides == 8) {
	if (r == dr)
	    res = window.ns_res;
	else if (c == dc)
	    res = window.ew_res;
	else
	    res = diag;
    }
    else {			/* sides == 4 */

	cseg_get(&asp, &asp_value, dr, dc);
	if (r == dr) {
	    if (asp_value == 2 || asp_value == 6)
		res = window.ns_res;
	    else		/* asp_value == 4, 8, -2, -4, -6, or -8 */
		res = diag;     /* how can res be diag with sides == 4??? */
	}
	else {			/* c == dc */

	    if (asp_value == 4 || asp_value == 8)
		res = window.ew_res;
	    else		/* asp_value == 2, 6, -2, -4, -6, or -8 */
		res = diag;
	}
    }
    dseg_get(&s_l, &top_ls, r, c);
    if (top_ls == half_res)
	top_ls = res;
    else
	top_ls += res;
    dseg_put(&s_l, &top_ls, r, c);
    cseg_get(&alt, &top_alt, r, c);
    cseg_get(&alt, &bot_alt, dr, dc);
    if (top_alt > bot_alt) {
	dseg_get(&s_l, &bot_ls, dr, dc);
	if (top_ls > bot_ls) {
	    bot_ls = top_ls + res;
	    dseg_put(&s_l, &bot_ls, dr, dc);
	    cseg_get(&r_h, &ridge, r, c);
	    cseg_put(&r_h, &ridge, dr, dc);
	}
    }

    return 0;
}
