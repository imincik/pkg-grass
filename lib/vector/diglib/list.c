/*
 ****************************************************************************
 *
 * MODULE:       Vector library 
 *              
 * AUTHOR(S):    Original author CERL, probably Dave Gerdes.
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
#include <stdlib.h>
#include <grass/gis.h>
#include <grass/Vect.h>

/* Init int_list */
int dig_init_list(struct ilist *list)
{
    list->value = NULL;
    list->n_values = 0;
    list->alloc_values = 0;

    return 1;
}

/* Init add item to list */
int dig_list_add(struct ilist *list, int val)
{
    void *p;
    int size;

    if (list->n_values == list->alloc_values) {
	size = (list->n_values + 1000) * sizeof(int);
	p = G_realloc((void *)list->value, size);
	if (p == NULL)
	    return 0;
	list->value = (int *)p;
	list->alloc_values = list->n_values + 1000;
    }

    list->value[list->n_values] = val;
    list->n_values++;

    return 1;
}
