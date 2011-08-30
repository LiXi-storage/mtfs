# Hrfs coreutils lib Makefile

AM_CFLAGS = $(LLCFLAGS)
AM_CPPFLAGS = $(LLCPPFLAGS) -DGNULIB_DIRNAME=1
AM_LDFLAGS := 

if UTILS
noinst_LIBRARIES = libcoreutils.a
endif # UTILS

libcoreutils_a_SOURCES = acl.h acl-internal.h areadlink.c areadlink.h areadlink-with-size.c
libcoreutils_a_SOURCES += basename-lgpl.c bitrotate.h buffer-lcm.c buffer-lcm.h copy-acl.c
libcoreutils_a_SOURCES += cp-hash.c cp-hash.h dirent--.h dirent-safer.h dirname.c
libcoreutils_a_SOURCES += dirname.h dirname-lgpl.c file-set.c file-set.h full-write.c
libcoreutils_a_SOURCES += full-write.h gettext.h gettime.c hash.c hash.h
libcoreutils_a_SOURCES += hash-pjw.c hash-pjw.h hash-triple.c hash-triple.h ignore-value.h
libcoreutils_a_SOURCES += opendir-safer.c quotearg.c quotearg.h quote.c quote.h
libcoreutils_a_SOURCES += safe-read.c safe-read.h safe-write.c safe-write.h same.c
libcoreutils_a_SOURCES += same.h same-inode.h savedir.c savedir.h set-mode-acl.c
libcoreutils_a_SOURCES += stat-macros.h stat-time.h stripslash.c timespec.h unistd-safer.h
libcoreutils_a_SOURCES += utimens.c utimens.h xalloc-die.c xalloc.h xmalloc.c
