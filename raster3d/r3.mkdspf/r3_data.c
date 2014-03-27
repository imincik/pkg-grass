#include "vizual.h"
#include <grass/G3d.h>


#define NO_RESAMPLE


int r3read_level(void *g3map, G3D_Region * g3reg, file_info * Headfax,
		 float *data, int n_lev)
{
#ifdef NO_RESAMPLE
    G3d_getBlock(g3map, 0, 0, n_lev,
		 Headfax->xdim, Headfax->ydim, 1, (char *)data, FCELL_TYPE);
#else
    /* G3d_getBlockRegion might be handy */
    /*
       G3d_getAllignedVolume (map, originNorth, originWest, originBottom, 
       lengthNorth, lengthWest, lengthBottom, 
       nx, ny, nz, volumeBuf, type);
     */
    G3d_getAllignedVolume(g3map, g3reg->north, g3reg->west,
			  g3reg->top - n_lev * g3reg->tb_res,
			  g3reg->north - g3reg->south,
			  g3reg->east - g3reg->west,
			  g3reg->tb_res,
			  Headfax->xdim, Headfax->ydim, 1,
			  (char *)data, FCELL_TYPE);
#endif

    return 0;
}
