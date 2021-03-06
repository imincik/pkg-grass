MODULE_TOPDIR = ../../..

PACKAGE = "grasslibs"

include $(MODULE_TOPDIR)/include/Make/Grass.make
include $(MODULE_TOPDIR)/include/Make/Rules.make
include $(MODULE_TOPDIR)/include/Make/Doxygen.make

# doxygen:
DOXNAME=
DOXINPUT=grasspython.dox

MODULES = date gis gmath proj imagery vector display stats \
	dbmi g3d arraystats cluster trans vedit ogsf nviz

date_LIBS       = -l$(DATETIME_LIBNAME)
gis_LIBS        = -l$(GIS_LIBNAME)
gmath_LIBS      = -l$(GMATH_LIBNAME)
proj_LIBS       = -l$(GPROJ_LIBNAME) -l$(GIS_LIBNAME)
imagery_LIBS    = -l$(IMAGERY_LIBNAME)
#vector_LIBS     = -l$(VECTR_LIBNAME) -l$(DIG2_LIBNAME) -l$(GRAPH_LIBNAME) -l$(RTREE_LIBNAME) -l$(LINKM_LIBNAME) -l$(DBMI_LIBNAME)
vector_LIBS     = -l$(VECTR_LIBNAME)
display_LIBS    = -l$(DISPLAY_LIBNAME)
stats_LIBS      = -l$(STATS_LIBNAME)
dbmi_LIBS       = -l$(DBMIBASE_LIBNAME) -l$(DBMICLIENT_LIBNAME) -l$(DBMIDRIVER_LIBNAME)
g3d_LIBS        = -l$(G3D_LIBNAME)
arraystats_LIBS = -l$(ARRAYSTATS_LIBNAME)
cluster_LIBS    = -l$(CLUSTER_LIBNAME)
trans_LIBS      = -l$(TRANS_LIBNAME)
vedit_LIBS      = -l$(VEDIT_LIBNAME)
ogsf_LIBS       = -l$(OGSF_LIBNAME)
nviz_LIBS       = -l$(NVIZ_LIBNAME)

date_HDRS       = datetime.h P_datetime.h
gis_HDRS        = gis.h gisdefs.h
gmath_HDRS      = gmath.h
proj_HDRS       = gprojects.h
imagery_HDRS    = imagery.h imagedefs.h
vector_HDRS     = Vect.h vect/dig_structs.h vect/dig_defines.h
display_HDRS    = display.h
stats_HDRS      = stats.h
dbmi_HDRS       = dbmi.h proto_dbmi.h
g3d_HDRS        = G3d.h
arraystats_HDRS = arraystats.h
cluster_HDRS    = cluster.h
trans_HDRS      = transform.h
vedit_HDRS      = vedit.h
ogsf_HDRS       = ogsf_proto.h gstypes.h gsurf.h kftypes.h keyframe.h
nviz_HDRS       = nviz.h

proj_INC        = $(PROJINC)
vector_INC      = $(VECT_INC) $(VECT_CFLAGS)
vedit_INC       = $(VECT_INC) $(VECT_CFLAGS)

SED = sed
CTYPESGEN = ./ctypesgen.py
CTYPESFLAGS = --cpp "$(CC) -E $(CPPFLAGS) $(LFS_CFLAGS) $(EXTRA_CFLAGS) $(NLS_CFLAGS) $(DEFS) $(EXTRA_INC) $(INC)"
EXTRA_CLEAN_FILES := $(foreach M,$(MODULES),$(M).py) $(wildcard ctypesgencore/*.pyc) $(wildcard ctypesgencore/*/*.pyc)

ifneq ($(MINGW),)
INTLLIB = -lintl3
endif

include $(MODULE_TOPDIR)/include/Make/Python.make

PYDIR = $(ETC)/python
GDIR = $(PYDIR)/grass
DSTDIR = $(GDIR)/lib

PYFILES  := $(patsubst %,$(DSTDIR)/%.py,$(MODULES) __init__ ctypes_preamble ctypes_loader)
PYCFILES  := $(patsubst %,$(DSTDIR)/%.pyc,$(MODULES) __init__ ctypes_preamble ctypes_loader)
LPYFILES := $(patsubst %,%.py,$(MODULES))

ifneq ($(strip $(CTYPESGEN)),)
default:
	$(MAKE) $(DSTDIR)
	$(MAKE) $(LPYFILES) $(PYFILES) $(PYCFILES)
endif

$(DSTDIR)/%.py: %.py | $(DSTDIR)
	$(SED) -f fix.sed $< > $@

$(DSTDIR)/ctypes_%.py: %.py | $(DSTDIR)
	$(INSTALL_DATA) $< $@

define module_rule
$(1)_DEPS = $$(patsubst %.h,$(ARCH_INCDIR)/%.h,$$($(1)_HDRS))
$(1).py: $$($(1)_DEPS)
	$$(call run_grass,$(CTYPESGEN) $(CTYPESFLAGS) $$($(1)_LIBS) $$($(1)_INC) $(EXTRA_LIBS) $$($(1)_DEPS) -o $$@)
endef
$(foreach module,$(MODULES),$(eval $(call module_rule,$(module))))

$(PYDIR):
	$(MKDIR) $@

$(GDIR): | $(PYDIR)
	$(MKDIR) $@

$(DSTDIR): | $(GDIR)
	$(MKDIR) $@

.SECONDARY: $(patsubst %,%.py,$(MODULES))
