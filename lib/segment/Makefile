MODULE_TOPDIR = ../..

EXTRA_LIBS=$(GISLIB)

LIB_NAME = $(SEGMENT_LIBNAME)

LIB_OBJS = \
        address.o\
        flush.o\
        format.o\
        get.o\
        get_row.o\
        init.o\
        pagein.o\
        pageout.o\
        put.o\
        put_row.o\
        release.o\
        seek.o\
        setup.o

include $(MODULE_TOPDIR)/include/Make/Lib.make
include $(MODULE_TOPDIR)/include/Make/Doxygen.make

#compile if LFS Large File Support present:
ifneq ($(USE_LARGEFILES),)
	EXTRA_CFLAGS = -D_FILE_OFFSET_BITS=64
endif

default: lib

#doxygen:
DOXNAME=segment
