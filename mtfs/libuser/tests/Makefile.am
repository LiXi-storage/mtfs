#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

# MTFS libmtfs test Makefile
AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS)
AM_LDFLAGS :=

LIBMTFS_LIBS = $(top_builddir)/mtfs/libuser/libuser.a
if LIBMTFS_TESTS
noinst_PROGRAMS = err2str

noinst_PROGRAMS += test_rule_tree test_branch_bitmap
noinst_PROGRAMS += test_parse_option
noinst_PROGRAMS += test_interval_tree test_mlock
noinst_PROGRAMS += test_mlowerfs_bucket
noinst_PROGRAMS += test_mlowerfs_bucket_random
noinst_PROGRAMS += test_mchecksum

test_rule_tree_SOURCES = test_rule_tree.c
test_rule_tree_CFLAGS = $(LL_CFLAGS)
test_rule_tree_LDADD := $(LIBMTFS_LIBS)
test_rule_tree_DEPENDENCIES := $(LIBMTFS_LIBS)

test_branch_bitmap_SOURCES = test_branch_bitmap.c
test_branch_bitmap_CFLAGS = $(LL_CFLAGS)
test_branch_bitmap_LDADD := $(LIBMTFS_LIBS)
test_branch_bitmap_DEPENDENCIES := $(LIBMTFS_LIBS)

test_parse_option_SOURCES = test_parse_option.c
test_parse_option_CFLAGS = $(LL_CFLAGS)
test_parse_option_LDADD := $(LIBMTFS_LIBS)
test_parse_option_DEPENDENCIES := $(LIBMTFS_LIBS)

test_interval_tree_SOURCES = test_interval_tree.c
test_interval_tree_CFLAGS = $(LL_CFLAGS)
test_interval_tree_LDADD := $(LIBMTFS_LIBS)
test_interval_tree_DEPENDENCIES := $(LIBMTFS_LIBS)

test_mlock_SOURCES = test_mlock.c
test_mlock_CFLAGS = $(LL_CFLAGS)
test_mlock_LDADD := $(LIBMTFS_LIBS) -lpthread
test_mlock_DEPENDENCIES := $(LIBMTFS_LIBS)

test_mlowerfs_bucket_SOURCES = test_mlowerfs_bucket.c
test_mlowerfs_bucket_CFLAGS = $(LL_CFLAGS)
test_mlowerfs_bucket_LDADD := $(LIBMTFS_LIBS)
test_mlowerfs_bucket_DEPENDENCIES := $(LIBMTFS_LIBS)

test_mlowerfs_bucket_random_SOURCES = test_mlowerfs_bucket_random.c
test_mlowerfs_bucket_random_CFLAGS = $(LL_CFLAGS)
test_mlowerfs_bucket_random_LDADD := $(LIBMTFS_LIBS)
test_mlowerfs_bucket_random_DEPENDENCIES := $(LIBMTFS_LIBS)

test_mchecksum_SOURCES = test_mchecksum.c
test_mchecksum_CFLAGS = $(LL_CFLAGS)
test_mchecksum_LDADD := $(LIBMTFS_LIBS) $(ZLIB)
test_mchecksum_DEPENDENCIES := $(LIBMTFS_LIBS)

endif #LIBMTFS_TESTS

noinst_DATA = 
noinst_SCRIPTS =
EXTRA_DIST = run.sh misc.sh
EXTRA_DIST += branch_bitmap cfg interval_tree manage
EXTRA_DIST += mchecksum mlowerfs_bucket mlowerfs_bucket_random
EXTRA_DIST += parse_option rule_tree
