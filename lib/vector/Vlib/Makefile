MODULE_TOPDIR = ../../..

PACKAGE ="libvect"
DEFS=-DPACKAGE=\"$(PACKAGE)\"

EXTRA_INC = $(VECT_INC)
EXTRA_CFLAGS = $(GETHOSTNAME) $(ZLIBINCPATH) $(VECT_CFLAGS) $(DEFS)
EXTRA_LIBS=$(GISLIB) $(DIG2LIB) $(GRAPHLIB) $(RTREELIB) $(LINKMLIB) $(DBMILIB) $(GDALLIBS) $(MATHLIB) $(GEOSLIBS)

LIB_NAME = $(VECTR_LIBNAME)
DEPENDENCIES =  $(ARCH_INCDIR)/Vect.h $(ARCH_INCDIR)/V_.h \
		$(ARCH_INCDIR)/vect/dig_defines.h \
		$(ARCH_INCDIR)/vect/dig_macros.h $(ARCH_INCDIR)/vect/dig_structs.h \
		$(ARCH_INCDIR)/vect/dig_externs.h $(ARCH_INCDIR)/vect/dig_globs.h $(GISDEP)

include $(MODULE_TOPDIR)/include/Make/Lib.make

default: lib

