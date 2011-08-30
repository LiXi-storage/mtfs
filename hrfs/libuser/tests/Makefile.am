#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

# Hrfs libhrfs test Makefile
AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS)
AM_LDFLAGS :=

LIBHRFS_LIBS = $(top_builddir)/hrfs/libuser/libuser.a
if LIBHRFS_TESTS
noinst_PROGRAMS = err2str

noinst_PROGRAMS += test_rule_tree test_branch_bitmap test_parse_option

test_rule_tree_SOURCES = test_rule_tree.c
test_rule_tree_CFLAGS = $(LL_CFLAGS)
test_rule_tree_LDADD := $(LIBHRFS_LIBS)
test_rule_tree_DEPENDENCIES := $(LIBHRFS_LIBS)

test_branch_bitmap_SOURCES = test_branch_bitmap.c
test_branch_bitmap_CFLAGS = $(LL_CFLAGS)
test_branch_bitmap_LDADD := $(LIBHRFS_LIBS)
test_branch_bitmap_DEPENDENCIES := $(LIBHRFS_LIBS)

test_parse_option_SOURCES = test_parse_option.c
test_parse_option_CFLAGS = $(LL_CFLAGS)
test_parse_option_LDADD := $(LIBHRFS_LIBS)
test_parse_option_DEPENDENCIES := $(LIBHRFS_LIBS)
endif #LIBHRFS_TESTS

noinst_DATA = 
noinst_SCRIPTS =
EXTRA_DIST = run.sh misc.sh
EXTRA_DIST += rule_tree
