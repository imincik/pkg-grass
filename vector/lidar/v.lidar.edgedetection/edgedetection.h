
/**************************************************************
 *									
 * MODULE:       v.lidar.edgedetection				
 * 								
 * AUTHOR(S):    Original version in GRASS 5.4 (s.edgedetection):
 * 		 Maria Antonia Brovelli, Massimiliano Cannata, 
 *		 Ulisse Longoni and Mirko Reguzzoni
 *
 *		 Update for GRASS 6.X and improvements:
 * 		 Roberto Antolin and Gonzalo Moreno
 *               							
 * PURPOSE:      Detection of object's edges on a LIDAR data set	
 *               							
 * COPYRIGHT:    (C) 2006 by Politecnico di Milano - 			
 *			     Polo Regionale di Como			
 *									
 *               This program is free software under the 		
 *               GNU General Public License (>=v2). 			
 *               Read the file COPYING that comes with GRASS		
 *               for details.					
 *							
 **************************************************************/

 /*INCLUDES*/
#include <grass/gis.h>
#include <grass/Vect.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>
#include <grass/PolimiFunct.h>

/*---------------------------------------------------------------------------------------*/
int edge_detection(struct Cell_head, /**/
		   BOUND_BOX, /**/
		   double *, /**/
		   double, /**/
		   double, /**/
		   double *, /**/
		   double, /**/ double, /**/ double, /**/ double /**/);

int Get_Gradient(struct Cell_head, /**/
		     double, /**/ double, /**/ double *, /**/ double *);

void classification(struct Map_info *, /**/
		    struct Cell_head, /**/
		    BOUND_BOX, /**/
		    BOUND_BOX, /**/
		    double **, /**/
		    double *, /**/
		    double *, /**/
		    double, /**/
		    double, /**/
		    double, /**/
		    double, /**/
		    double, /**/
		    int *, /**/ int, /**/ dbDriver *, /**/ char *, /**/ char *);

int Insert(double, /**/ double, /**/ double, /**/ int, /**/ dbDriver *, /**/ char *);

int UpDate(double, /**/ double, /**/ double, /**/ int, /**/ dbDriver *, /**/ char *);

int Select(double *, /**/ double *, /**/ double *, /**/ int, /**/ dbDriver *, /**/ char *);

int Insert_Interpolation(double, int, dbDriver *, char *);
