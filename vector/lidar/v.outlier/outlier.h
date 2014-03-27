#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>

#include <grass/PolimiFunct.h>

/*--------------------------------------------------------------------------*/
/*FUNCTIONS DECLARATION */
void
P_Outlier(struct Map_info *, /**/
	  struct Map_info *, /**/
	  struct Map_info *, /**/
	  struct Cell_head, /**/
	  BOUND_BOX, /**/
	  BOUND_BOX, /**/
	  double **, /**/
	  double *, /**/
	  double, /**/ double, /**/ int *, /**/ int, /**/ dbDriver *,  /**/ char *);

int Insert_Outlier(double, /**/ int, /**/ dbDriver *, /**/ char *);

int UpDate_Outlier(double, /**/ int, /**/ dbDriver *, /**/ char *);

int Select_Outlier(double *, /**/ int, /**/ dbDriver *, /**/ char *);

int P_is_outlier(double);
