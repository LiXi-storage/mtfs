AM_CPPFLAGS = $(LLCPPFLAGS)
AM_CFLAGS = $(LLCFLAGS)
LIBMTFS_LIBS = $(top_builddir)/mtfs/libuser/libuser.a
LDADD = -lpthread $(LIBMTFS_LIBS)
DEPENDENCIES = $(LIBMTFS_LIBS)
# LDADD = -lldap
# LDADD := -lreadline -ltermcap # -lefence

noinst_DATA =

noinst_SCRIPTS = socketclient.pl socketserver.pl

nobase_noinst_SCRIPTS = 

nobase_noinst_DATA = 

EXTRA_DIST = $(noinst_SCRIPTS) $(noinst_DATA) \
             $(nobase_noinst_SCRIPTS) $(nobase_noinst_DATA)

if TESTS
noinst_PROGRAMS = checkstat createmany createtest
noinst_PROGRAMS += directio flocks_test mcreate
noinst_PROGRAMS += mmap_sanity mrename multifstat
noinst_PROGRAMS += multiop munlink opendevunlink
noinst_PROGRAMS += opendirunlink openfile
noinst_PROGRAMS += openfilleddirunlink openunlink
noinst_PROGRAMS += rename_many runas small_write
noinst_PROGRAMS += statmany truncate unlinkmany utime
noinst_PROGRAMS += mlink rwv sendfile correct multicorrect

testsrcdir = $(libdir)/mtfs/tests/src
testsrc_SCRIPTS = $(noinst_SCRIPTS) $(noinst_PROGRAMS)
nobase_testsrc_SCRIPTS = $(nobase_noinst_SCRIPTS)
testsrc_DATA = $(noinst_DATA)
nobase_testsrc_DATA = $(nobase_noinst_DATA)
endif
