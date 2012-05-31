# Filesystem management Makefile
SUBDIRS := .

AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS) -DMTFS_MANAGE=1
AM_LDFLAGS := 

LIBUSER := $(top_builddir)/mtfs/libuser/libuser.a
LIBCOREUTILS := $(top_builddir)/mtfs/utils/libcoreutils/libcoreutils.a

sbin_scripts =  
bin_scripts = 

if MANAGE

sbin_PROGRAMS = 
bin_PROGRAMS = hfsm
bin_SCRIPTS = $(bin_scripts)
sbin_SCRIPTS = $(sbin_scripts)
rootsbin_PROGRAMS =

endif # MANAGE

lib_LIBRARIES = 
hfsm_SOURCES = hfsm.c fsmL_internal.h component.c component_internal.h workspace.c workspace_internal.h graphviz.c graphviz_internal.h
hfsm_LDADD := $(LIBREADLINE) $(LIBUSER) $(LIBLUA)
hfsm_DEPENDENCIES := $(LIBUSER)

EXTRA_DIST = $(sbin_scripts) $(bin_scripts)
