#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

# Administration utilities Makefile
SUBDIRS := libcoreutils .

AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS) -DHRFS_UTILS=1
AM_LDFLAGS := -lattr

LIBUSER := $(top_builddir)/hrfs/libuser/libuser.a
LIBCOREUTILS := $(top_builddir)/hrfs/utils/libcoreutils/libcoreutils.a

sbin_scripts =  
bin_scripts = 

if UTILS

sbin_PROGRAMS = hrfsctl
bin_PROGRAMS = hrfs
bin_SCRIPTS = $(bin_scripts)
sbin_SCRIPTS = $(sbin_scripts)

# mount only finds helpers in /sbin
rootsbin_PROGRAMS = mount.hrfs

endif # UTILS

lib_LIBRARIES = libhrfsapi.a

hrfsctl_SOURCES = hctl.c
hrfsctl_LDADD := $(LIBREADLINE) $(LIBUSER)
hrfsctl_DEPENDENCIES := 

hrfs_SOURCES = hrfs.c repair.c repair.h cp.c cp.h hide.c hide.h grouplock.c grouplock.h exit_status.h
hrfs_LDADD := $(LIBREADLINE) libhrfsapi.a $(LIBUSER) $(LIBCOREUTILS)
hrfs_DEPENDENCIES := $(LIBUSER) $(LIBCOREUTILS) libhrfsapi.a

libhrfsapi_a_SOURCES = libhrfsapi.c

mount_hrfs_SOURCES = mount_hrfs.c
mount_hrfs_LDADD := $(LIBUSER)
mount_hrfs_DEPENDENCIES := $(LIBUSER)

EXTRA_DIST = $(sbin_scripts) $(bin_scripts) libhrfsapi.h
