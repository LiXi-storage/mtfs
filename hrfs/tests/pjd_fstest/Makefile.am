AM_CPPFLAGS = $(LLCPPFLAGS) -DHAS_ACL
AM_CFLAGS = $(LLCFLAGS) -DHAS_ACL
#CFLAGS+=-DHAS_ACL -lacl
LDADD = -lacl

noinst_DATA =

noinst_SCRIPTS = 

nobase_noinst_SCRIPTS = tests/conf tests/misc.sh
nobase_noinst_SCRIPTS += tests/chflags/* tests/chmod/* tests/chown/*
nobase_noinst_SCRIPTS += tests/link/* tests/mkdir/* tests/mkfifo/* 
nobase_noinst_SCRIPTS += tests/open/* tests/rename/* tests/rmdir/*
nobase_noinst_SCRIPTS += tests/symlink/* tests/truncate/*
nobase_noinst_SCRIPTS += tests/unlink/* tests/xacl

nobase_noinst_DATA =

EXTRA_DIST = $(noinst_SCRIPTS) $(noinst_DATA) \
             $(nobase_noinst_SCRIPTS) $(nobase_noinst_DATA)
if TESTS
noinst_PROGRAMS = fstest

pjd_fstestdir = $(libdir)/hrfs/tests/pjd_fstest
pjd_fstest_SCRIPTS = $(noinst_SCRIPTS) $(noinst_PROGRAMS)
nobase_pjd_fstest_SCRIPTS = $(nobase_noinst_SCRIPTS)
pjd_fstest_DATA = $(noinst_DATA)
nobase_pjd_fstest_DATA = $(nobase_noinst_DATA)
endif
