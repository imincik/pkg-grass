MODULE_TOPDIR = ..

SUBDIRS = \
	r.average \
	r.basins.fill \
	r.bilinear \
	r.bitpattern \
	r.buffer \
	r.carve \
	r.cats \
	r.circle \
	r.clump \
	r.coin \
	r.colors \
	r.colors.out \
	r.composite \
	r.compress \
	r.contour \
	r.cost \
	r.covar \
	r.cross \
	r.describe \
	r.distance \
	r.drain \
	r.external \
	r.fill.dir \
	r.flow \
	r.grow2 \
	r.grow.distance \
	r.gwflow \
	r.horizon \
	r.his \
	r.in.arc \
	r.in.ascii \
	r.in.bin \
	r.info \
	r.in.gridatb \
	r.in.mat \
	r.in.poly \
	r.in.xyz \
	r.kappa \
	r.lake \
	r.le \
	r.los \
	r.mapcalc \
	r.median \
	r.mfilter \
	r.mfilter.fp \
	r.mode \
	r.neighbors \
	r.null \
	r.out.arc \
	r.out.ascii \
	r.out.bin \
	r.out.gridatb \
	r.out.mat \
	r.out.mpeg \
	r.out.pov \
	r.out.ppm \
	r.out.ppm3 \
	r.out.vrml \
	r.out.vtk \
	r.param.scale \
	r.patch \
	r.profile \
	r.proj.seg \
	r.quant \
	r.quantile \
	r.random \
	r.random.cells \
	r.random.surface \
	r.reclass \
	r.recode \
	r.region \
	r.regression.line \
	r.report \
	r.resample \
	r.resamp.interp \
	r.resamp.rst \
	r.resamp.stats \
	r.rescale \
	r.rescale.eq \
	r.series \
	r.slope.aspect \
	r.statistics \
	r.stats \
	r.sum \
	r.sun2 \
	r.sunmask \
	r.support \
	r.support.stats \
	r.surf.area \
	r.surf.contour \
	r.surf.idw \
	r.surf.idw2 \
	r.surf.random \
	r.surf.gauss \
	r.texture \
	r.thin \
	r.timestamp \
	r.to.rast3 \
	r.to.rast3elev \
	r.to.vect \
	r.topidx \
	r.topmodel \
	r.transect \
	r.univar2 \
	r.volume \
	r.walk \
	r.water.outlet \
	r.watershed \
	r.what \
	r.what.color \
	simwe \
	wildfire

GDALBASED = r.in.gdal r.out.gdal

FFTWBASED = r.surf.fractal

PNGBASED = r.out.png

CXXBASED = r.terraflow

TIFFBASED = r.out.tiff

XMONBASED = r.digit


include $(MODULE_TOPDIR)/include/Make/Platform.make

ifndef MINGW
    SUBDIRS += r.li
endif

#compile if GDAL present:
ifneq ($(USE_GDAL),)
    SUBDIRS += $(GDALBASED)
endif

#compile if FFTW present:
ifneq ($(strip $(FFTWLIB)),)
    SUBDIRS += $(FFTWBASED)
endif

#compile if PNG present:
ifneq ($(strip $(PNGLIB)),)
    SUBDIRS += $(PNGBASED)
endif

#compile if C++ compiler present:
ifneq ($(strip $(CXX)),)
    SUBDIRS += $(CXXBASED)
endif

#compile if TIFF present:
ifneq ($(strip $(TIFFLIB)),)
    SUBDIRS += $(TIFFBASED)
endif

#compile if interactive Xmons are present:
ifneq ($(USE_X11),)
    SUBDIRS += $(XMONBASED)
endif


PGM = rasterintro

include $(MODULE_TOPDIR)/include/Make/Dir.make

default: htmldir

htmldir: parsubdirs 

clean: cleansubdirs
