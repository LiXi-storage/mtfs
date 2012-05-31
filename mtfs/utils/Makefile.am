# Administration utilities Makefile
SUBDIRS := .

AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS) -DMTFS_UTILS=1
AM_LDFLAGS := -lattr

LIBUSER := $(top_builddir)/mtfs/libuser/libuser.a

sbin_scripts =  
bin_scripts = 

if UTILS

sbin_PROGRAMS = mtfsctl
bin_PROGRAMS = mtfs
bin_SCRIPTS = $(bin_scripts)
sbin_SCRIPTS = $(sbin_scripts)

# mount only finds helpers in /sbin
rootsbin_PROGRAMS = mount.mtfs

endif # UTILS

lib_LIBRARIES = libmtfsapi.a

mtfsctl_SOURCES = mtfsctl.c
mtfsctl_LDADD := $(LIBREADLINE) $(LIBUSER) libmtfsapi.a
mtfsctl_DEPENDENCIES := $(LIBUSER) libmtfsapi.a

mtfs_SOURCES = mtfs.c hide.c hide.h grouplock.c grouplock.h exit_status.h
mtfs_LDADD := $(LIBREADLINE) libmtfsapi.a $(LIBUSER)
mtfs_DEPENDENCIES := $(LIBUSER) libmtfsapi.a

libmtfsapi_a_SOURCES = libmtfsapi.c

mount_mtfs_SOURCES = mount_mtfs.c
mount_mtfs_LDADD := $(LIBUSER)
mount_mtfs_DEPENDENCIES := $(LIBUSER)

EXTRA_DIST = $(sbin_scripts) $(bin_scripts) libmtfsapi.h
