#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "G3d_intern.h"

/*---------------------------------------------------------------------------*/

static int cacheRead_readFun(int tileIndex, void *tileBuf, void *closure)
{
    G3D_Map *map = closure;

    if (!G3d_readTile(map, tileIndex, tileBuf, map->typeIntern)) {
	G3d_error("cacheRead_readFun: error in G3d_readTile");
	return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------*/

static int initCacheRead(G3D_Map * map, int nCached)
{
    map->cache = G3d_cache_new_read(nCached,
				    map->tileSize * map->numLengthIntern,
				    map->nTiles, cacheRead_readFun, map);
    if (map->cache == NULL) {
	G3d_error("initCacheRead: error in G3d_cache_new_read");
	return 0;
    }

    return 1;
}

/*---------------------------------------------------------------------------*/

/*
   the map->index array is (ab)used to store the positions of the tiles in the 
   file-cash. we can do this since we maintain the invariant for every tile 
   that it is either in no file (index == -1) or in either the output-file
   (index >= 0) or the cash-file (index <= -2). to convert the file-position in 
   the cash-file into an index we use the following function:

   index = - (fileposition + 2)

   symmetrically, we use

   fileposition = - (index + 2)

   to convert from index to the fileposition.
 */

/*---------------------------------------------------------------------------*/

static int cacheWrite_readFun(int tileIndex, void *tileBuf, void *closure)
{
    G3D_Map *map = closure;
    int index, nBytes;
    long pos, offs, offsLast;

    pos = map->index[tileIndex];

    /* tile has already been flushed onto output file or does not exist yet */
    if (pos >= -1) {		/* note, G3d_readTile takes care of the case pos == -1 */
	G3d_readTile(map, tileIndex, tileBuf, map->typeIntern);
	return 1;
    }

    /* tile is in cache file */

    pos = -pos - 2;		/* pos is shifted by 2 to avoid 0 and -1 */

    nBytes = map->tileSize * map->numLengthIntern;
    offs = pos * (nBytes + sizeof(int));

    /* seek tile and read it into buffer */

    if (lseek(map->cacheFD, offs, SEEK_SET) == -1) {
	G3d_error("cacheWrite_readFun: can't position file");
	return 0;
    }
    if (read(map->cacheFD, tileBuf, nBytes) != nBytes) {
	G3d_error("cacheWrite_readFun: can't read file");
	return 0;
    }

    /* remove it from index */

    map->index[tileIndex] = -1;

    /* if it is the last tile in the file we are done */
    /* map->cachePosLast tells us the position of the last tile in the file */

    if (map->cachePosLast == pos) {
	map->cachePosLast--;
	return 1;
    }

    /* otherwise we move the last tile in the file into the position of */
    /* the tile we just read and update the hash information */

    offsLast = map->cachePosLast * (nBytes + sizeof(int));

    if (lseek(map->cacheFD, offsLast, SEEK_SET) == -1) {
	G3d_error("cacheWrite_readFun: can't position file");
	return 0;
    }
    if (read(map->cacheFD, xdr, nBytes + sizeof(int)) != nBytes + sizeof(int)) {
	G3d_error("cacheWrite_readFun: can't read file");
	return 0;
    }

    if (lseek(map->cacheFD, offs, SEEK_SET) == -1) {
	G3d_error("cacheWrite_readFun: can't position file");
	return 0;
    }
    if (write(map->cacheFD, xdr, nBytes + sizeof(int)) !=
	nBytes + sizeof(int)) {
	G3d_error("cacheWrite_readFun: can't write file");
	return 0;
    }

    index = *((int *)((unsigned char *)xdr + nBytes));
    map->index[index] = -pos - 2;

    map->cachePosLast--;

    return 1;
}

/*---------------------------------------------------------------------------*/

static int
cacheWrite_writeFun(int tileIndex, const void *tileBuf, void *closure)
{
    G3D_Map *map = closure;
    int nBytes;
    long offs;

    if (map->index[tileIndex] != -1)
	return 1;

    map->cachePosLast++;
    nBytes = map->tileSize * map->numLengthIntern;
    offs = map->cachePosLast * (nBytes + sizeof(int));

    if (lseek(map->cacheFD, offs, SEEK_SET) == -1) {
	G3d_error("cacheWrite_writeFun: can't position file");
	return 0;
    }
    if (write(map->cacheFD, tileBuf, nBytes) != nBytes) {
	G3d_error("cacheWrite_writeFun: can't write file");
	return 0;
    }
    if (write(map->cacheFD, &tileIndex, sizeof(int)) != sizeof(int)) {
	G3d_error("cacheWrite_writeFun: can't write file");
	return 0;
    }

    map->index[tileIndex] = -map->cachePosLast - 2;

    return 1;
}

/*---------------------------------------------------------------------------*/

static int disposeCacheWrite(G3D_Map * map)
{
    if (map->cacheFD >= 0) {
	if (close(map->cacheFD) != 0) {
	    G3d_error("disposeCacheWrite: could not close file");
	    return 0;
	}
	remove(map->cacheFileName);
	G3d_free(map->cacheFileName);
    }

    G3d_cache_dispose(map->cache);

    return 1;
}

/*---------------------------------------------------------------------------*/

static int initCacheWrite(G3D_Map * map, int nCached)
{
    map->cacheFileName = G_tempfile();
    map->cacheFD = open(map->cacheFileName, O_RDWR | O_CREAT | O_TRUNC, 0666);

    if (map->cacheFD < 0) {
	G3d_error("initCacheWrite: could not open file");
	return 0;
    }

    map->cachePosLast = -1;

    map->cache = G3d_cache_new(nCached,
			       map->tileSize * map->numLengthIntern,
			       map->nTiles,
			       cacheWrite_writeFun, map,
			       cacheWrite_readFun, map);

    if (map->cache == NULL) {
	disposeCacheWrite(map);
	G3d_error("initCacheWrite: error in G3d_cache_new");
	return 0;
    }

    return 1;
}

/*---------------------------------------------------------------------------*/

int G3d_initCache(G3D_Map * map, int nCached)
{
    if (map->operation == G3D_READ_DATA) {
	if (!initCacheRead(map, nCached)) {
	    G3d_error("G3d_initCache: error in initCacheRead");
	    return 0;
	}
	return 1;
    }

    if (!initCacheWrite(map, nCached)) {
	G3d_error("G3d_initCache: error in initCacheWrite");
	return 0;
    }

    return 1;
}

/*---------------------------------------------------------------------------*/

static int disposeCacheRead(G3D_Map * map)
{
    G3d_cache_dispose(map->cache);
    return 1;
}

/*---------------------------------------------------------------------------*/

int G3d_disposeCache(G3D_Map * map)
{
    if (map->operation == G3D_READ_DATA) {
	if (!disposeCacheRead(map)) {
	    G3d_error("G3d_disposeCache: error in disposeCacheRead");
	    return 0;
	}
	return 1;
    }

    if (!disposeCacheWrite(map)) {
	G3d_error("G3d_disposeCache: error in disposeCacheWrite");
	return 0;
    }

    return 1;
}


/*---------------------------------------------------------------------------*/

static int cacheFlushFun(int tileIndex, const void *tileBuf, void *closure)
{
    G3D_Map *map = closure;

    if (!G3d_writeTile(map, tileIndex, tileBuf, map->typeIntern)) {
	G3d_error("cacheFlushFun: error in G3d_writeTile");
	return 0;
    }

    return 1;
}

/*---------------------------------------------------------------------------*/

int G3d_flushAllTiles(G3D_Map * map)
{
    int tileIndex, nBytes;
    long offs;

    if (map->operation == G3D_READ_DATA) {
	if (!G3d_cache_remove_all(map->cache)) {
	    G3d_error("G3d_flushAllTiles: error in G3d_cache_remove_all");
	    return 0;
	}
	return 1;
    }

    /* make cache write into output file instead of cache file */
    G3d_cache_set_removeFun(map->cache, cacheFlushFun, map);

    /* first flush all the tiles which are in the file cache */

    nBytes = map->tileSize * map->numLengthIntern;

    while (map->cachePosLast >= 0) {
	offs = map->cachePosLast * (nBytes + sizeof(int)) + nBytes;

	if (lseek(map->cacheFD, offs, SEEK_SET) == -1) {
	    G3d_error("G3d_flushAllTiles: can't position file");
	    return 0;
	}
	if (read(map->cacheFD, &tileIndex, sizeof(int)) != sizeof(int)) {
	    G3d_error("G3d_flushAllTiles: can't read file");
	    return 0;
	}

	if (!G3d_cache_load(map->cache, tileIndex)) {
	    G3d_error("G3d_flushAllTiles: error in G3d_cache_load");
	    return 0;
	}
	if (!G3d_cache_flush(map->cache, tileIndex)) {
	    G3d_error("G3d_flushAllTiles: error in G3d_cache_flush");
	    return 0;
	}
    }

    /* then flush all the tiles which remain in the non-file cache */
    if (!G3d_cache_flush_all(map->cache)) {
	G3d_error("G3d_flushAllTiles: error in G3d_cache_flush_all");
	return 0;
    }

    /* now the cache should write into the cache file again */
    G3d_cache_set_removeFun(map->cache, cacheWrite_writeFun, map);

    return 1;
}
