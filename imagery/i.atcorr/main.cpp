/***************************************************************************
                 atcorr - atmospheric correction for Grass GIS

                                    was
        6s - Second Simulation of Satellite Signal in the Solar Spectrum.
                             -------------------
    begin                : Fri Jan 10 2003
    copyright            : (C) 2003 by Christo Zietsman
    email                : 13422863@sun.ac.za

    This program has been rewriten in C/C++ from the fortran source found
    on http://www.ltid.inpe.br/dsr/mauro/6s/index.html. This code is
    provided as is, with no implied warranty and under the conditions
    and restraint as placed on it by previous authors.

    atcorr is an atmospheric correction module for Grass GIS.  Limited testing
    has been done and therefore it should not be assumed that it will work
    for all possible inputs.

    Care has been taken to test new functionality brought to the module by
    Grass such as the command-line options and flags. The extra feature of
    supplying an elevation map has not been run to completion, because it
    takes to long and no sensible data for the test data was at hand.
    Testing would be welcomed. :)  
**********

* Code clean-up and port to GRASS 6.3, 15.12.2006:
  Yann Chemin, ychemin(at)gmail.com 

* Addition of IRS-1C LISS, Feb 2009: Markus Neteler

* input elevation/visibility map: efficient cache with dynamic memory 
* allocation: Markus Metz, Apr 2011

***************************************************************************/

#include <cstdlib>
#include <map>

extern "C" {
#include <grass/gis.h>
#include <grass/glocale.h>
}

#include "Transform.h"
#include "6s.h"
#include "rbtree.h"

/* TICache: create 1 meter bins for altitude in km */
/* 10m bins are also ok */
/* BIN_ALT = 1000 / <bin size in meters> */
#define BIN_ALT 1000.
/* TICache: create 1 km bins for visibility */
#define BIN_VIS 1.

/* uncomment to disable cache usage */
/* #define _NO_OPTIMIZE_ */

/* Input options and flags */
struct Options
{
    /* options */
    struct Option *iimg;    /* input satellite image */
    struct Option *iscl;    /* input data is scaled to this range */
    struct Option *ialt;    /* an input elevation map in meters used to increase */
                            /* atmospheric correction accuracy, including this */
                            /* will make computations take much, much longer */
    struct Option *ivis;    /* an input visibility map in km (same purpose and effect as ialt) */
    struct Option *icnd;    /* the input conditions file */
    struct Option *oimg;    /* output image name */
    struct Option *oscl;    /* scale the output data (reflectance values) to this range */

    /* flags */
    struct Flag *oflt;      /* output data as floating point and do not round */
    struct Flag *irad;      /* treat input values as reflectance instead of radiance values */
    struct Flag *etmafter;  /* treat input data as a satelite image of type etm+ taken after July 1, 2000 */
    struct Flag *etmbefore; /* treat input data as a satelite image of type etm+ taken before July 1, 2000 */
    struct Flag *optimize;
};

struct ScaleRange
{
    int min;
    int max;
};

struct RBitem
{
    int alt;		/* elevation */
    int vis;		/* visibility */
    TransformInput ti;	/* transformation parameters */
};

/* function prototypes */
static void adjust_region(char *, const char *);
static CELL round_c(FCELL);
static void write_fp_to_cell(int, FCELL *);
static void process_raster(int, InputMask, ScaleRange, int, int, int, bool, ScaleRange);
static void copy_colors(char *, const char *, char *);
static void define_module(void);
static struct Options define_options(void);
static void read_scale(Option *, ScaleRange &);


/* 
   Adjust the region to that of the input raster.
   Atmospheric corrections should be done on the whole
   satelite image, not just portions.
*/
static void adjust_region(char *name, const char *mapset)
{
    struct Cell_head iimg_head;	/* the input image header file */

    if(G_get_cellhd(name, mapset, &iimg_head) < 0) 
	G_fatal_error (_("Unable to read header of raster map <%s>"),
		       G_fully_qualified_name(name, mapset));

    if(G_set_window(&iimg_head) < 0) 
	G_fatal_error (_("Invalid graphics region coordinates"));
}


/* Rounds a floating point cell value */
static CELL round_c(FCELL x)
{
    if (x >= 0.0)
	return (CELL)(x + .5);

    return (CELL)(-(-x + .5));
}


/* Converts the buffer to cell and write it to disk */
static void write_fp_to_cell(int ofd, FCELL* buf)
{
    CELL* cbuf;
    int col;

    cbuf = (CELL*)G_allocate_raster_buf(CELL_TYPE);

    for(col = 0; col < G_window_cols(); col++) cbuf[col] = round_c(buf[col]);
    G_put_raster_row(ofd, cbuf, CELL_TYPE);
}

/* compare function for RB tree */
static int compare_hv(const void *ti_a, const void *ti_b)
{
    struct RBitem *a, *b;

    a = (struct RBitem *)ti_a;
    b = (struct RBitem *)ti_b;

    /* check most common case first
     * also faster if either an altitude or a visibility map is given,
     * but not both */
    if (a->alt == b->alt) {
	if (a->vis == b->vis)
	    return 0;
	if (a->vis > b->vis)
	    return 1;
	else if (a->vis < b->vis)
	    return -1;
    }
    else if (a->alt > b->alt)
	return 1;
    else if (a->alt < b->alt)
	return -1;

    /* should not be reached */
    return 0;
}

/* Cache for transformation input parameters */
class TICache
{
    struct RB_TREE *RBTree;
    unsigned int tree_size;

  private:
    struct RBitem set_alt_vis(float alt, float vis)
    {
	struct RBitem rbitem;

	/* although alt and vis are already rounded,
	 * the + 0.5 is needed for fp representation errors */
	/* alt has been converted to kilometers */
	rbitem.alt = (int) (alt * BIN_ALT + 0.5);
	/* vis should be kilometers */
	rbitem.vis = (int) (vis + 0.5);
	
	return rbitem;
    }

  public:
      TICache()
    {
	RBTree = rbtree_create(compare_hv, sizeof(struct RBitem));
	tree_size = 0;
    }
    int search(float alt, float vis, TransformInput *ti)
    {
	struct RBitem search_ti = set_alt_vis(alt, vis);
	struct RBitem *found_ti;

	found_ti = (struct RBitem *)rbtree_find(RBTree, &search_ti);
	if (found_ti) {
	    *ti = found_ti->ti;
	    return 1;
	}
	else
	    return 0;

    }

    void add(TransformInput ti, float alt, float vis)
    {
	struct RBitem insert_ti = set_alt_vis(alt, vis);

	/* add safety check here */
	tree_size++;

	insert_ti.ti = ti;

	rbtree_insert(RBTree, &insert_ti);
    }
};


/* Process the raster and do atmospheric corrections.
   Params:
   * INPUT FILE
   ifd: input file descriptor
   iref: input file has radiance values (default is reflectance) ?
   iscale: input file's range (default is min = 0, max = 255)
   ialt_fd: height map file descriptor, negative if global value is used
   ivis_fd: visibility map file descriptor, negative if global value is used

   * OUTPUT FILE
   ofd: output file descriptor
   oflt: if true use FCELL_TYPE for output
   oscale: output file's range (default is min = 0, max = 255)
*/
static void process_raster(int ifd, InputMask imask, ScaleRange iscale,
			    int ialt_fd, int ivis_fd, int ofd, bool oflt,
			    ScaleRange oscale)
{
    FCELL* buf;         /* buffer for the input values */
    FCELL* alt = NULL;         /* buffer for the elevation values */
    FCELL* vis = NULL;         /* buffer for the visibility values */
    FCELL  prev_alt = -1.f;
    FCELL  prev_vis = -1.f;
    int row, col, nrows, ncols;
    /* switch on optimization automatically if elevation and/or visibility map is given */
    bool optimize = (ialt_fd >= 0 || ivis_fd >= 0);
    
#ifdef _NO_OPTIMIZE_
    optimize = false;
#endif

    /* do initial computation with global elevation and visibility values */
    TransformInput ti;
    ti = compute();

    /* use a cache to increase computation speed when an elevation map 
     * and/or a visibility map is given */
    TICache ticache;

    /* allocate memory for buffers */
    buf = (FCELL*)G_allocate_raster_buf(FCELL_TYPE);
    if(ialt_fd >= 0) alt = (FCELL*)G_allocate_raster_buf(FCELL_TYPE);
    if(ivis_fd >= 0) vis = (FCELL*)G_allocate_raster_buf(FCELL_TYPE);

    nrows = G_window_rows();
    ncols = G_window_cols();

    for(row = 0; row < nrows; row++)
    {
	G_percent(row, nrows, 1);     /* keep the user informed of our progress */
		
        /* read the next row */
	if(G_get_raster_row(ifd, buf, row, FCELL_TYPE) < 0)
	    G_fatal_error (_("Unable to read input raster map row %d"),
			     row);

        /* read the next row of elevation values */
        if(ialt_fd >= 0)
	    if(G_get_raster_row(ialt_fd, alt, row, FCELL_TYPE) < 0)
		G_fatal_error (_("Unable to read elevation raster map row %d"),
			       row);

        /* read the next row of elevation values */
        if(ivis_fd >= 0)
	    if(G_get_raster_row(ivis_fd, vis, row, FCELL_TYPE) < 0)
		G_fatal_error (_("Unable to read visibility raster map row %d"),
			       row);

        /* loop over all the values in the row */
	for(col = 0; col < ncols; col++)
	{
	    if ((vis && G_is_f_null_value(&vis[col])) || 
	       (alt && G_is_f_null_value(&alt[col])) || 
	              G_is_f_null_value(&buf[col]))
	    {
	        G_set_f_null_value(&buf[col], 1);
	        continue;
	    }
	    if (ialt_fd >= 0) {
		if (alt[col] < 0)
		    alt[col] = 0; /* on or below sea level, all the same for 6S */
		else
		    alt[col] /= 1000.0f;	/* converting to km from input which should be in meter */

		/* round to nearest altitude bin */
		/* rounding result: watch out for fp representation error */
		alt[col] = ((int) (alt[col] * BIN_ALT + 0.5)) / BIN_ALT;
	    }
	    if (ivis_fd >= 0) {
		if (vis[col] < 0)
		    vis[col] = 0; /* negative visibility is invalid, print a WARNING ? */

		/* round to nearest visibility bin */
		/* rounding result: watch out for fp representation error */
		vis[col] = ((int) (vis[col] + 0.5));
	    }

            /* check if both maps are active and if whether any value has changed */
            if((ialt_fd >= 0) && (ivis_fd >= 0) && ((prev_vis != vis[col]) || (prev_alt != alt[col])))
            {
               	prev_alt = alt[col]; /* update new values */
               	prev_vis = vis[col];
		if (optimize) {
		    int in_cache = ticache.search(alt[col], vis[col], &ti);

		    if (!in_cache) {
			pre_compute_hv(alt[col], vis[col]);	/* re-compute transformation inputs */
			ti = compute();	/* ... */

			ticache.add(ti, alt[col], vis[col]);
		    }
		}
		else { /* no optimizations */
		    pre_compute_hv(alt[col], vis[col]);
		    ti = compute();
		}	
            }
            else    /* only one of the maps is being used */
            {
                if((ivis_fd >= 0) && (prev_vis != vis[col]))
                {
                    prev_vis = vis[col];        /* keep track of previous visibility */
                    
		    if (optimize) {
			int in_cache = ticache.search(0, vis[col], &ti);

			if (!in_cache) {
			    pre_compute_v(vis[col]);	/* re-compute transformation inputs */
			    ti = compute();	/* ... */

			    ticache.add(ti, 0, vis[col]);
			}
		    }
                    else
                    {
                        pre_compute_v(vis[col]);    /* re-compute transformation inputs */
                        ti = compute();             /* ... */
                    }
                }

                if((ialt_fd >= 0) && (prev_alt != alt[col]))
                {
                    prev_alt = alt[col];        /* keep track of previous altitude */

		    if (optimize) {
			int in_cache = ticache.search(alt[col], 0, &ti);

			if (!in_cache) {
			    pre_compute_h(alt[col]);	/* re-compute transformation inputs */
			    ti = compute();	/* ... */

			    ticache.add(ti, alt[col], 0);
			}
		    }
                    else
                    {
                        pre_compute_h(alt[col]);    /* re-compute transformation inputs */
                        ti = compute();             /* ... */
                    }
                }
            }
	    G_debug(3, "Computed r%d (%d), c%d (%d)", row, nrows, col, ncols);
            /* transform from iscale.[min,max] to [0,1] */
            buf[col] = (buf[col] - iscale.min) / ((float)iscale.max - (float)iscale.min);
            buf[col] = transform(ti, imask, buf[col]);
            /* transform from [0,1] to oscale.[min,max] */
            buf[col] = buf[col] * ((float)oscale.max - (float)oscale.min) + oscale.min;

            if(~oflt && (buf[col] > (float)oscale.max))
		G_warning(_("The output data will overflow. Reflectance > 100%%"));
	}

        /* write output */
	if(oflt) G_put_raster_row(ofd, buf, FCELL_TYPE);
	else write_fp_to_cell(ofd, buf);
    }
    G_percent(1, 1, 1);
    
    /* free allocated memory */
    G_free(buf);
    if(ialt_fd >= 0) G_free(alt);
    if(ivis_fd >= 0) G_free(vis);
}



/* Copy the colors from map named iname to the map named oname */
static void copy_colors(char *iname, const char *imapset, char *oname)
{
    struct Colors colors;

    G_read_colors(iname, imapset, &colors);
    G_write_colors(oname, G_mapset(), &colors);
}


/* Define our module so that Grass can print it if the user wants to know more. */
static void define_module(void)
{
    struct GModule *module;

    module = G_define_module();
    module->label = _("Performs atmospheric correction using the 6S algorithm.");
    module->description =
	_("6S - Second Simulation of Satellite Signal in the Solar Spectrum.");
    module->keywords = _("imagery, atmospheric correction");

    /* 
       " Incorporated into Grass by Christo A. Zietsman, January 2003.\n"
       " Converted from Fortran to C by Christo A. Zietsman, November 2002.\n\n"
       " Adapted by Mauro A. Homem Antunes for atmopheric corrections of\n"
       " remotely sensed images in raw format (.RAW) of 8 bits.\n"
       " April 4, 2001.\n\n"
       " Please refer to the following paper and acknowledge the authors of\n"
       " the model:\n"
       " Vermote, E.F., Tanre, D., Deuze, J.L., Herman, M., and Morcrette,\n"
       "    J.J., (1997), Second simulation of the satellite signal in\n"
       "    the solar spectrum, 6S: An overview., IEEE Trans. Geosc.\n"
       "    and Remote Sens. 35(3):675-686.\n"
       " The code is provided as is and is not to be sold. See notes on\n"
       " http://loasys.univ-lille1.fr/informatique/sixs_gb.html\n"
       " http://www.ltid.inpe.br/dsr/mauro/6s/index.html\n"
       " and on http://www.cs.sun.ac.za/~caz/index.html\n";*/
}


/* Define the options and flags */
static struct Options define_options(void)
{
    struct Options opts;

    opts.iimg = G_define_standard_option(G_OPT_R_INPUT);
    opts.iimg->key = "iimg";
    
    opts.iscl = G_define_option();
    opts.iscl->key          = "iscl";
    opts.iscl->type         = TYPE_INTEGER;
    opts.iscl->key_desc     = "min,max";
    opts.iscl->required     = NO;
    opts.iscl->answer       = "0,255";
    opts.iscl->description  = _("Input imagery range [0,255]");
    opts.iscl->guisection = _("Input");

    opts.ialt = G_define_standard_option(G_OPT_R_INPUT);
    opts.ialt->key		= "ialt";
    opts.ialt->required	        = NO;
    opts.ialt->description	= _("Input altitude raster map in m (optional)");
    opts.ialt->guisection       = _("Input");

    opts.ivis = G_define_standard_option(G_OPT_R_INPUT);
    opts.ivis->key		= "ivis";
    opts.ivis->required	        = NO;
    opts.ivis->description	= _("Input visibility raster map in km (optional)");
    opts.ivis->guisection       = _("Input");

    opts.icnd = G_define_standard_option(G_OPT_F_INPUT);
    opts.icnd->key		= "icnd";
    opts.icnd->required	        = YES;
    opts.icnd->description	= _("Name of input text file");

    opts.oimg = G_define_standard_option(G_OPT_R_OUTPUT);
    opts.oimg->key		= "oimg";

    opts.oscl = G_define_option();
    opts.oscl->key          = "oscl";
    opts.oscl->type         = TYPE_INTEGER;
    opts.oscl->key_desc     = "min,max";
    opts.oscl->answer       = "0,255";
    opts.oscl->required     = NO;
    opts.oscl->description  = _("Rescale output raster map [0,255]");
    opts.oscl->guisection = _("Output");

    opts.oflt = G_define_flag();
    opts.oflt->key = 'f';
    opts.oflt->description = _("Output raster is floating point");
    opts.oflt->guisection = _("Output");

    opts.irad = G_define_flag();
    opts.irad->key = 'r';
    opts.irad->description = _("Input map converted to reflectance (default is radiance)");
    opts.irad->guisection = _("Input");

    opts.etmafter = G_define_flag();
    opts.etmafter->key = 'a';
    opts.etmafter->description = _("Input from ETM+ image taken after July 1, 2000");
    opts.etmafter->guisection = _("Input");

    opts.etmbefore = G_define_flag();
    opts.etmbefore->key = 'b';
    opts.etmbefore->description = _("Input from ETM+ image taken before July 1, 2000");
    opts.etmbefore->guisection = _("Input");

    opts.optimize = G_define_flag();
    opts.optimize->key = 'o';
    opts.optimize->description =
	_("Try to increase computation speed when altitude and/or visibility map is used");

    return opts;
}

/* Read the min and max values from the iscl and oscl options */
void read_scale(Option *scl, ScaleRange &range)
{
    /* set default values */
    range.min = 0;
    range.max = 255;

    if(scl->answer)
    {
        sscanf(scl->answers[0], "%d", &range.min);
        sscanf(scl->answers[1], "%d", &range.max);

        if(range.min==range.max)
        {
            G_warning(_("Scale range length should be > 0; Using default values: [0,255]"));

            range.min = 0;
            range.max = 255;
        }
    }

    /* swap values if max is smaller than min */
    if(range.max < range.min)
    {
        int temp;
        temp = range.max;
        range.max = range.min;
        range.min = temp;
    }
}


int main(int argc, char* argv[])
{
    struct Options opts;        
    struct ScaleRange iscale;   /* input file's data is scaled to this interval */
    struct ScaleRange oscale;   /* output file's scale */
    int iimg_fd;	        /* input image's file descriptor */
    int oimg_fd;	        /* output image's file descriptor */
    int ialt_fd = -1;       /* input elevation map's file descriptor */
    int ivis_fd = -1;       /* input visibility map's file descriptor */
    const char *iimg_mapset, *ialt_mapset, *iviz_mapset;
    struct History hist;
    struct Cell_head orig_window;
    
    /* Define module */
    define_module();
  
    /* Define the different input options */
    opts = define_options();

    /**** Start ****/
    G_gisinit(argv[0]);
    if (G_parser(argc, argv) < 0)
	exit(EXIT_FAILURE);

    /* open input raster */
    if ( (iimg_mapset = G_find_cell2 ( opts.iimg->answer, "") ) == NULL )
	G_fatal_error ( _("Raster map <%s> not found"), opts.iimg->answer);
    if((iimg_fd = G_open_cell_old(opts.iimg->answer, iimg_mapset)) < 0)
	G_fatal_error(_("Unable to open raster map <%s>"),
		       G_fully_qualified_name(opts.iimg->answer, iimg_mapset));

    G_get_set_window(&orig_window);
    adjust_region(opts.iimg->answer, iimg_mapset);
        
    if(opts.ialt->answer) {
	if ( (ialt_mapset = G_find_cell2 ( opts.ialt->answer, "") ) == NULL )
	    G_fatal_error ( _("Raster map <%s> not found"), opts.ialt->answer);
	if((ialt_fd = G_open_cell_old(opts.ialt->answer, ialt_mapset)) < 0)
            G_fatal_error(_("Unable to open raster map <%s>"),
			   G_fully_qualified_name(opts.ialt->answer, ialt_mapset));
    }

    if(opts.ivis->answer) {
	if ( (iviz_mapset = G_find_cell2 ( opts.ivis->answer, "") ) == NULL )
	    G_fatal_error ( _("Raster map <%s> not found"), opts.ivis->answer);
	if((ivis_fd = G_open_cell_old(opts.ivis->answer, iviz_mapset)) < 0)
            G_fatal_error(_("Unable to open raster map <%s>"),
			   G_fully_qualified_name(opts.ivis->answer, iviz_mapset));
    }
                
    /* open a floating point raster or not? */
    if(opts.oflt->answer)
    {
	if((oimg_fd = G_open_fp_cell_new(opts.oimg->answer)) < 0)
	    G_fatal_error(_("Unable to create raster map <%s>"),
			   opts.oimg->answer);
    }
    else
    {
	if((oimg_fd = G_open_raster_new(opts.oimg->answer, CELL_TYPE)) < 0)
	    G_fatal_error(_("Unable to create raster map <%s>"),
			   opts.oimg->answer);
    }

    /* read the scale parameters */
    read_scale(opts.iscl, iscale);
    read_scale(opts.oscl, oscale);

    /* initialize this 6s computation and parse the input conditions file */
    init_6S(opts.icnd->answer);
	
    InputMask imask = RADIANCE;         /* the input mask tells us what transformations if any
					   needs to be done to make our input values, reflectance
					   values scaled between 0 and 1 */
    if(opts.irad->answer) imask = REFLECTANCE;
    if(opts.etmbefore->answer) imask = (InputMask)(imask | ETM_BEFORE);
    if(opts.etmafter->answer) imask = (InputMask)(imask | ETM_AFTER);

    /* switch on optimization automatically if elevation and/or visibility map is given */
    if (opts.optimize->answer)
	G_important_message(_("Optimization is switched on automatically, the -o flag has no effect"));

    /* process the input raster and produce our atmospheric corrected output raster. */
    G_message(_("Atmospheric correction..."));
    process_raster(iimg_fd, imask, iscale, ialt_fd, ivis_fd,
                   oimg_fd, opts.oflt->answer, oscale);


    /* Close the input and output file descriptors */
    G_short_history(opts.oimg->answer, "raster", &hist);
    G_close_cell(iimg_fd);
    if(opts.ialt->answer) G_close_cell(ialt_fd);
    if(opts.ivis->answer) G_close_cell(ivis_fd);
    G_close_cell(oimg_fd);

    G_command_history(&hist);
    G_write_history(opts.oimg->answer, &hist);

    /* Copy the colors of the input raster to the output raster.
       Scaling is ignored and color ranges might not be correct. */
    copy_colors(opts.iimg->answer, iimg_mapset, opts.oimg->answer);

    G_set_window(&orig_window);
    G_message(_("Atmospheric correction complete."));

    exit(EXIT_SUCCESS);
}
