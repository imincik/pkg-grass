#ifndef __WATERGLOBS_H__
#define __WATERGLOBS_H__

#define EPS     1.e-7
#define MAXW    7000000
#define UNDEF	-9999

/*
extern FILE *fdelevin, *fddxin, *fddyin, *fdrain, *fdinfil, *fdtraps,
    *fdmanin, *fddepth, *fddisch, *fderr, *fdoutwalk, *fdwalkers;
*/
extern FILE *fdelevin, *fddxin, *fddyin, *fdrain, *fdinfil, *fdtraps,
    *fdmanin, *fddepth, *fddisch, *fderr;
extern FILE *fdwdepth, *fddetin, *fdtranin, *fdtauin, *fdtc, *fdet, *fdconc,
    *fdflux, *fderdep;
extern FILE *fdsfile, *fw;

extern char *elevin;
extern char *dxin;
extern char *dyin;
extern char *rain;
extern char *infil;
extern char *traps;
extern char *manin;
/* extern char *sfile; */
extern char *depth;
extern char *disch;
extern char *err;
/* extern char *outwalk; */
extern char *mapset;
extern char *mscale;
extern char *tserie;

extern char *wdepth;
extern char *detin;
extern char *tranin;
extern char *tauin;
extern char *tc;
extern char *et;
extern char *conc;
extern char *flux;
extern char *erdep;

extern char *rainval;
extern char *maninval;
extern char *infilval;

struct options
{
    struct Option *elevin, *dxin, *dyin, *rain, *infil, *traps, *manin,
	*sfile, *depth, *disch, *err, *outwalk, *nwalk, *niter, *outiter,
	*density, *diffc, *hmax, *halpha, *hbeta, *wdepth, *detin, *tranin,
	*tauin, *tc, *et, *conc, *flux, *erdep, *rainval, *maninval,
	*infilval;
};

extern struct options parm;

struct flags
{
    struct Flag *mscale, *tserie;
};

extern struct flags flag;

struct seed
{
    long int is1, is2;
};

extern struct seed seed;


extern struct Cell_head cellhd;

struct Point
{
    double north, east;
    double z1;
};

/*
extern struct Point *points;
extern int npoints;
extern int npoints_alloc;
*/

extern int input_data(void);
extern int seeds(long int, long int);
extern int seedg(long int, long int);
extern int grad_check(void);
extern void erod(double **);
extern void main_loop(void);
extern int output_data(int, double);
extern int output_et(void);
extern double ulec(void);
extern double gasdev(void);
extern double amax1(double, double);
extern double amin1(double, double);
extern int min(int, int);
extern int max(int, int);

extern double xmin, ymin, xmax, ymax;
extern double mayy, miyy, maxx, mixx;
extern int mx, my;
extern int mx2, my2;

extern double bxmi, bymi, bxma, byma, bresx, bresy;
extern int maxwab;
extern double step, conv;

extern double frac;
extern double bxmi, bymi;

extern float **zz, **cchez;
extern double **v1, **v2, **slope;
extern double **gama, **gammas, **si, **inf, **sigma;
extern float **dc, **tau, **er, **ct, **trap;
extern float **dif;

/* extern double vavg[MAXW][2], stack[MAXW][3], w[MAXW][3]; */
extern double vavg[MAXW][2], w[MAXW][3];
extern int iflag[MAXW];

extern double hbeta;
/* extern int ldemo; */
extern double hhmax, sisum, vmean;
extern double infsum, infmean;
extern int maxw, maxwa, nwalk;
extern double rwalk, bresx, bresy, xrand, yrand;
extern double stepx, stepy, xp0, yp0;
extern double chmean, si0, deltap, deldif, cch, hhc, halpha;
extern double eps;
/* extern int maxwab, nstack; */
extern int maxwab;
extern int iterout, mx2o, my2o;
extern int miter, nwalka, lwwfin;
extern double timec;
extern int ts, timesec;

extern double rain_val;
extern double manin_val;
extern double infil_val;

extern struct History history;	/* holds meta-data (title, comments,..) */

#endif /* __WATERGLOBS_H__ */
