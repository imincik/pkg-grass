MODULE_TOPDIR = ../..

SUBDIRS = docs
EXTRA_CLEAN_FILES = menustrings.py build_ext.pyc

include $(MODULE_TOPDIR)/include/Make/Dir.make
include $(MODULE_TOPDIR)/include/Make/Doxygen.make
include $(MODULE_TOPDIR)/include/Make/Python.make

ETCDIR = $(ETC)/wxpython

SRCFILES := $(wildcard compat/* icons/*.* scripts/*.py xml/*) \
	$(wildcard core/* dbmgr/* gcp/* gmodeler/* gui_core/* lmgr/* location_wizard/* \
	mapdisp/* modules/* nviz/* psmap/* vdigit/* wxplot/* ) \
	gis_set.py wxgui.py README
DSTFILES := $(patsubst %,$(ETCDIR)/%,$(SRCFILES)) $(patsubst %.py,$(ETCDIR)/%.pyc,$(filter %.py,$(SRCFILES)))

PYDSTDIRS := $(patsubst %,$(ETCDIR)/%,core dbmgr gcp gmodeler gui_core lmgr location_wizard \
	mapdisp modules nviz psmap vdigit wxplot)
DSTDIRS := $(patsubst %,$(ETCDIR)/%,compat icons scripts xml)

default: $(DSTFILES) menustrings.py
	$(MAKE) parsubdirs

$(ETCDIR)/%: % | $(PYDSTDIRS) $(DSTDIRS)
	$(INSTALL_DATA) $< $@

menustrings.py: core/menudata.py $(ETCDIR)/xml/menudata.xml $(ETCDIR)/xml/menudata_modeler.xml 
	$(call run_grass,$(PYTHON) $< > $@)
	$(call run_grass,$(PYTHON) $< "modeler" >> $@)

$(PYDSTDIRS): %: | $(ETCDIR)
	$(MKDIR) $@
	$(call run_grass,$(PYTHON) create__init__.py $@)

$(DSTDIRS): %: | $(ETCDIR)
	$(MKDIR) $@

$(ETCDIR):
	$(MKDIR) $@

#doxygen:
DOXNAME=wxpython
