#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "G3d_intern.h"

/*---------------------------------------------------------------------------*/


/*!
 * \brief 
 *
 * Assumes that <em>tile</em> is a tile with the same dimensions as the
 * tiles of <em>map</em>. Fills <em>tile</em> with NULL-values of
 * <em>type</em>.
 *
 *  \param map
 *  \param tile
 *  \param type
 *  \return void
 */

void G3d_setNullTileType(G3D_Map * map, void *tile, int type)
{
    G3d_setNullValue(tile, map->tileSize, type);
}

/*---------------------------------------------------------------------------*/


/*!
 * \brief 
 *
 * Is equivalent to G3d_setNullTileType (map, tile, G3d_fileTypeMap (map)).
 *
 *  \param map
 *  \param tile
 *  \return void
 */

void G3d_setNullTile(G3D_Map * map, void *tile)
{
    G3d_setNullTileType(map, tile, map->typeIntern);
}
