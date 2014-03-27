#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "G3d_intern.h"

/*---------------------------------------------------------------------------*/

void
G3d_getBlockNocache(G3D_Map * map, int x0, int y0, int z0, int nx, int ny,
		    int nz, void *block, int type)
{
    void *tile;
    int tileX0, tileY0, tileZ0, tileOffsX0, tileOffsY0, tileOffsZ0;
    int tileX1, tileY1, tileZ1, tileOffsX1, tileOffsY1, tileOffsZ1;
    int tx, ty, tz, dx, dy, dz, x, y, z, rows, cols, depths;
    int tileIndex;

    if (!map->useCache)
	tile = G3d_allocTilesType(map, 1, type);
    if (tile == NULL)
	G3d_fatalError("G3d_getBlockNocache: error in G3d_allocTiles");

    G3d_coord2tileCoord(map, x0, y0, z0, &tileX0, &tileY0, &tileZ0,
			&tileOffsX0, &tileOffsY0, &tileOffsZ0);
    G3d_coord2tileCoord(map, x0 + nx - 1, y0 + ny - 1, z0 + nz - 1,
			&tileX1, &tileY1, &tileZ1,
			&tileOffsX1, &tileOffsY1, &tileOffsZ1);

    for (tz = tileZ0; tz <= tileZ1; tz++) {
	dz = (tz - tileZ0) * map->tileZ - tileOffsZ0;
	for (ty = tileY0; ty <= tileY1; ty++) {
	    dy = (ty - tileY0) * map->tileY - tileOffsY0;
	    for (tx = tileX0; tx <= tileX1; tx++) {
		dx = (tx - tileX0) * map->tileX - tileOffsX0;

		tileIndex = G3d_tile2tileIndex(map, tx, ty, tz);

		if (G3d_tileIndexInRange(map, tileIndex))
		    if (map->useCache) {
			tile = G3d_getTilePtr(map, tileIndex);
			if (tile == NULL)
			    G3d_fatalError
				("G3d_getBlockNocache: error in G3d_getTilePtr");
		    }
		    else {
			if (!G3d_readTile
			    (map, tileIndex, tile, map->typeIntern))
			    G3d_fatalError
				("G3d_getBlockNocache: error in G3d_readTile");
		    }

		else
		    G3d_setNullTile(map, tile);

		cols = (tx == tileX1 ? tileOffsX1 : map->tileX - 1);
		rows = (ty == tileY1 ? tileOffsY1 : map->tileY - 1);
		depths = (tz == tileZ1 ? tileOffsZ1 : map->tileZ - 1);

		x = (tx == tileX0 ? tileOffsX0 : 0);

		for (z = (tz == tileZ0 ? tileOffsZ0 : 0); z <= depths; z++)
		    for (y = (ty == tileY0 ? tileOffsY0 : 0); y <= rows; y++) {
			G3d_copyValues(tile,
				       z * map->tileXY + y * map->tileX + x,
				       map->typeIntern,
				       block,
				       (z + dz) * nx * ny + (y + dy) * nx +
				       (x + dx), type, cols - x + 1);
		    }
	    }
	}
    }

    if (!map->useCache)
	G3d_freeTiles(tile);
}

/*---------------------------------------------------------------------------*/


/*!
 * \brief 
 *
 * Copies the cells contained in the block (cube) with vertices 
 * <em>(x0, y0, z0)</em> and <em>(x0 + nx - 1, y0 + ny - 1, z0 + nz - 1)</em>
 * into <em>block</em>. The cell-values in <em>block</em> are of <em>type</em>.
 * The source code can be found in <em>getblock.c</em>.
 *
 *  \param map
 *  \param x0
 *  \param y0
 *  \param z0
 *  \param nx
 *  \param ny
 *  \param nz
 *  \param block
 *  \param type
 *  \return void
 */

void
G3d_getBlock(G3D_Map * map, int x0, int y0, int z0, int nx, int ny, int nz,
	     void *block, int type)
{
    int x, y, z, nNull, x1, y1, z1, length;

    if (!map->useCache) {
	G3d_getBlockNocache(map, x0, y0, z0, nx, ny, nz, block, type);
	return;
    }

    x1 = G3D_MIN(x0 + nx, map->region.cols);
    y1 = G3D_MIN(y0 + ny, map->region.rows);
    z1 = G3D_MIN(z0 + nz, map->region.depths);

    length = G3d_length(type);

    for (z = z0; z < z1; z++) {
	for (y = y0; y < y1; y++) {
	    for (x = x0; x < x1; x++) {
		G3d_getValueRegion(map, x, y, z, block, type);
		block = G_incr_void_ptr(block, length);
	    }
	    nNull = x0 + nx - x;
	    G3d_setNullValue(block, nNull, type);
	    block = G_incr_void_ptr(block, length * nNull);
	}
	nNull = (y0 + ny - y) * nx;
	G3d_setNullValue(block, nNull, type);
	block = G_incr_void_ptr(block, length * nNull);
    }
    nNull = (z0 + nz - z) * ny * nx;
    G3d_setNullValue(block, nNull, type);
}
