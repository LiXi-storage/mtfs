#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

# MTFS user lib Makefile
SUBDIRS := . tests

AM_CFLAGS = $(LLCFLAGS)
AM_CPPFLAGS = $(LLCPPFLAGS)
AM_LDFLAGS :=

USER_LIBS = $(top_builddir)/mtfs/mtfs/libmtfs.a libuser_local.a

if LIBMTFS
lib_LIBRARIES = libuser.a
noinst_LIBRARIES = libuser_local.a

install-exec-hook: libuser.so
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	@list=$< ; for p in $$list; do \
		if test -f $$p; then \
			f="`echo $$p | sed -e 's|^.*/||'`"; \
			echo " $(libLIBRARIES_INSTALL) $$p $(DESTDIR)$(libdir)/$$f"; \
			$(libLIBRARIES_INSTALL) $$p $(DESTDIR)$(libdir)/$$f; \
		else :; fi; \
	done
else #!LIBMTFS
install-exec-hook:
endif #!LIBMTFS

libuser_a_SOURCES = 
libuser_local_a_SOURCES = parser.c cmd_parser.c log.c multithread.c cfg.c

libuser.a : $(USER_LIBS)
	sh $(srcdir)/genlib.sh
EXTRA_DIST = genlib.sh
CLEANFILES := libuser.so
